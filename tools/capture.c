#include <poll.h>
#include <unistd.h>
#include <signal.h>
#include <sys/signalfd.h>
#include <sys/wait.h>

#define LIBR_IMPLEMENTATION
#include "r.h"

struct buffer {
    uint8_t* bs;
    size_t L;
    size_t i;
};

void buffer_init(struct buffer* b, size_t L)
{
    b->bs = calloc(1, L); CHECK_MALLOC(b->bs);
    b->L = L;
    b->i = 0;
}

int buffer_full(const struct buffer* b)
{
    return b->i == b->L;
}

int buffer_empty(const struct buffer* b)
{
    return b->i == 0;
}

void buffer_fill(struct buffer* b, int fd)
{
    while(1) {
        if(buffer_full(b)) {
            return;
        }

        const size_t l = b->L - b->i;
        ssize_t s = read(fd, &b->bs[b->i], l);
        if(s == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            return;
        }
        CHECK(s, "read(%d, .., %zu)", fd, l);

        debug("read(%d) = %zd", fd, s);

        b->i += s;
    }
}

void buffer_discard(int fd)
{
    debug("discarding from: %d", fd);
    while(1) {
        uint8_t bs[1024];
        const size_t l = LENGTH(bs);
        ssize_t s = read(fd, bs, l);
        if(s == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            return;
        }
        CHECK(s, "read(%d, .., %zu)", fd, l);

        debug("discarding %zd from %d", s, fd);
    }
}

void buffer_drain(struct buffer* b, int fd)
{
    while(1) {
        if(buffer_empty(b)) {
            return;
        }

        const size_t l = b->i;
        ssize_t s = write(fd, b->bs, l);
        if(s == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            return;
        }
        CHECK(s, "write(%d, .., %zu)", fd, l);

        debug("write(%d) = %zd", fd, s);

        size_t j = s - l;
        memmove(b->bs, &b->bs[s], j);
        b->i = j;
    }
}

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

    struct buffer out;
    int stdout_hup;
    struct buffer err;
    int stderr_hup;
} state;

int should_continue(void)
{
    if(state.child_terminated
       && state.stdout_hup && buffer_empty(&state.out)
       && state.stderr_hup && buffer_empty(&state.err)) {
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

        r = dup2(stdout_pair[1], 1); CHECK(r, "dup2(.., 1)");
        r = close(stdout_pair[0]); CHECK(r, "close");

        r = close(stderr_pair[0]); CHECK(r, "close");
        r = dup2(stderr_pair[1], 2); CHECK(r, "dup2(.., 2)");

        // argv[argc] == NULL
        // https://www.gnu.org/software/libc/manual/html_node/Program-Arguments.html
        r = execvp(argv[offset], &argv[offset]);
        CHECK(r, "execv");
    }

    info("spawned child: %d", state.child);

    r = close(0); CHECK(r, "close(0)");
    r = close(stdout_pair[1]); CHECK(r, "close");
    r = close(stderr_pair[1]); CHECK(r, "close");

    set_blocking(1, 0);
    set_blocking(2, 0);
    set_blocking(stdout_pair[0], 0);
    set_blocking(stderr_pair[0], 0);

    struct pollfd fds[] = {
        { .fd = sfd, .events = POLLIN },
        { .fd = stdout_pair[0], .events = 0 },
        { .fd = stderr_pair[0], .events = 0 },
        { .fd = 1, .events = 0 },
        { .fd = 2, .events = 0 },
    };

    buffer_init(&state.out, 4096);
    buffer_init(&state.err, 4096);

    while(should_continue()) {
        if(!state.stdout_hup && !buffer_full(&state.out)) {
            fds[1].events = POLLIN;
        } else {
            fds[1].events = 0;
        }

        if(!state.stderr_hup && !buffer_full(&state.err)) {
            fds[2].events = POLLIN;
        } else {
            fds[2].events = 0;
        }

        if(!buffer_empty(&state.out)) {
            fds[3].events = POLLOUT;
        } else {
            fds[3].events = 0;
        }

        if(!buffer_empty(&state.err)) {
            fds[4].events = POLLOUT;
        } else {
            fds[4].events = 0;
        }

        debug("poll");
        int r = poll(fds, LENGTH(fds), -1);
        CHECK_IF(r < 1, "poll");

        if(fds[0].revents & POLLIN) {
            handle_signalfd(fds[0].fd);
            fds[0].revents ^= POLLIN;
        }

        if(fds[1].revents & POLLIN) {
            debug("stdout in");
            if(o.silence_stdout) {
                buffer_discard(fds[1].fd);
            } else {
                buffer_fill(&state.out, fds[1].fd);
            }
            fds[1].revents ^= POLLIN;
        }

        if(fds[1].revents & POLLHUP) {
            debug("stdout hup");
            state.stdout_hup = 1;
            fds[1].revents ^= POLLHUP;
        }

        if(fds[2].revents & POLLIN) {
            debug("stderr in");
            if(o.silence_stderr) {
                buffer_discard(fds[2].fd);
            } else {
                buffer_fill(&state.err, fds[2].fd);
            }
            fds[2].revents ^= POLLIN;
        }
        if(fds[2].revents & POLLHUP) {
            debug("stderr hup");
            state.stderr_hup = 1;
            fds[2].revents ^= POLLHUP;
        }

        if(fds[3].revents & POLLOUT) {
            debug("stdout out");
            buffer_drain(&state.out, fds[3].fd);
            fds[3].revents ^= POLLOUT;
        }

        if(fds[4].revents & POLLOUT) {
            debug("stderr out");
            buffer_drain(&state.err, fds[4].fd);
            fds[4].revents ^= POLLOUT;
        }

        for(size_t i = 0; i < LENGTH(fds); i++) {
            if(fds[i].revents != 0) {
                failwith("unhandled events: fds[%zu].revents = %d",
                         i, fds[i].revents);
            }
        }
    }

    info("child returncode: %d", state.returncode);
    return state.returncode != 0;
}
