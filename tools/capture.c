#include <poll.h>
#include <unistd.h>
#include <signal.h>
#include <sys/signalfd.h>
#include <sys/wait.h>

#define LIBR_IMPLEMENTATION
#include "r.h"

struct options {
    int silence_stdout;
    const char* stdout_fn;
    int silence_stderr;
    const char* stderr_fn;
};

static void print_usage(int fd, const char* prog)
{
    dprintf(fd, "usage: %s [OPTION]... [COMMAND [ARG]...]\n", prog);
    dprintf(fd, "\n");
    dprintf(fd, "options:\n");
    dprintf(fd, "  -O       do not output stdout\n");
    dprintf(fd, "  -o FILE  write stdout to FILE\n");
    dprintf(fd, "  -E       do not output stderr\n");
    dprintf(fd, "  -e FILE  write stderr to FILE\n");
    dprintf(fd, "  -h       print this message\n");
}

static int parse_options(struct options* o, int argc, char* argv[])
{
    memset(o, 0, sizeof(*o));

    int res;
    while((res = getopt(argc, argv, "Oo:Ee:h-")) != -1) {
        switch(res) {
        case 'O':
            o->silence_stdout = 1;
            break;
        case 'o':
            o->stdout_fn = strdup(optarg);
            CHECK_NOT(o->stdout_fn, NULL, "strdup(%s)", optarg);
            break;
        case 'E':
            o->silence_stderr = 1;
            break;
        case 'e':
            o->stderr_fn = strdup(optarg);
            CHECK_NOT(o->stderr_fn, NULL, "strdup(%s)", optarg);
            break;
        case '-':
            goto opt_end;
        case 'h':
        default:
            print_usage(res == 'h' ? 1 : 2, argv[0]);
            exit(res == 'h' ? 0 : 1);
        }
    }

opt_end:

    if(optind == argc) {
        dprintf(2, "error: too few arguments\n");
        print_usage(2, argv[0]);
        exit(1);
    }

    return optind;
}

struct {
    pid_t child;
    int child_terminated;
    int returncode;
} state;

int should_continue(void)
{
    if(state.child_terminated) {
        return 0;
    }

    return 1;
}

void graceful_shutdown(const char* reason, int child_sig)
{
    if(reason) {
        info("initiating graceful shutdown: %s", reason);
    } else {
        info("initiating graceful shutdown");
    }

    if(!state.child_terminated) {
        info("child (%d) is still running; sending %s",
             state.child, strsignal(child_sig));
        int r = kill(state.child, child_sig);
        CHECK(r, "kill(%d, %s)", state.child, strsignal(child_sig));
    }
}

void handle_signalfd(int fd) {
    struct signalfd_siginfo si;
    ssize_t s = read(fd, &si, sizeof(si));
    if(s == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
        return;
    }
    CHECK(s, "read");
    if(s != sizeof(si)) {
        failwith("unexpected partial read");
    }

    if(si.ssi_signo == SIGCHLD) {
        if(state.child == 0) {
            failwith("unexpected SIGCHLD signal");
        }

        int ws;
        pid_t r = waitpid(state.child, &ws, WNOHANG);
        CHECK_IF(r != state.child, "waitpid(%d) != %d", state.child, r);

        if(WIFEXITED(ws)) {
            info("child (%d) exited: %d", state.child, WEXITSTATUS(ws));
            state.returncode = WEXITSTATUS(ws);
        } else if(WIFSIGNALED(ws)) {
            info("child (%d) signaled: %d", state.child, WTERMSIG(ws));
            state.returncode = -WTERMSIG(ws);
        } else {
            failwith("unexpected waitpid (%d) status: %d", state.child, ws);
        }
        state.child_terminated = 1;
    } else if(si.ssi_signo == SIGINT
              || si.ssi_signo == SIGQUIT
              || si.ssi_signo == SIGTERM) {
        graceful_shutdown(strsignal(si.ssi_signo), si.ssi_signo);
    } else {
        failwith("unexpected signal: %s", strsignal(si.ssi_signo));
    }

    handle_signalfd(fd);
}

int main(int argc, char* argv[])
{
    memset(&state, 0, sizeof(state));

    struct options o;
    int offset = parse_options(&o, argc, argv);

    for(int i = offset; i < argc; i++) {
        debug("cmdline[%d]=%s", i-offset, argv[i]);
    }

    sigset_t sm;
    sigemptyset(&sm);
    sigaddset(&sm, SIGINT);
    sigaddset(&sm, SIGQUIT);
    sigaddset(&sm, SIGTERM);
    sigaddset(&sm, SIGCHLD);
    int sfd = signalfd(-1, &sm, SFD_NONBLOCK | SFD_CLOEXEC);
    CHECK(sfd, "signalfd");

    int r = sigprocmask(SIG_BLOCK, &sm, NULL);
    CHECK(r, "sigprocmask");

    int stdout_pair[2];
    r = pipe(stdout_pair);

    int stderr_pair[2];
    r = pipe(stderr_pair);

    state.child = fork(); CHECK(state.child, "fork");
    if(state.child == 0) {
        r = sigprocmask(SIG_UNBLOCK, &sm, NULL);
        CHECK(r, "sigprocmask");

        r = close(sfd); CHECK(r, "close");

        r = dup2(stdout_pair[1], 1); CHECK(r, "dup2(.., 1)");
        r = close(stdout_pair[0]); CHECK(r, "close");

        r = close(stderr_pair[0]); CHECK(r, "close");
        r = dup2(stderr_pair[1], 2); CHECK(r, "dup2(.., 2)");

        // argv[argc] == NULL
        // https://www.gnu.org/software/libc/manual/html_node/Program-Arguments.html
        r = execvp(argv[offset], &argv[offset]);
        CHECK(r, "execv");
    }

    r = close(0); CHECK(r, "close(0)");

    info("spawned child: %d", state.child);

    struct pollfd fds[] = {
        { .fd = sfd, .events = POLLIN },
    };

    while(should_continue()) {
        int r = poll(fds, LENGTH(fds), -1);
        CHECK_IF(r < 1, "poll");

        if(fds[0].revents & POLLIN) {
            handle_signalfd(fds[0].fd);
        }
    }

    info("child returncode: %d", state.returncode);
    return state.returncode != 0;
}
