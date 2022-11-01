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

    const char* returncode_fn;
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
    dprintf(fd, "  -r FILE  write returncode to FILE (-SIG if signaled)\n");
    dprintf(fd, "  -h       print this message\n");
}

static int parse_options(struct options* o, int argc, char* argv[])
{
    memset(o, 0, sizeof(*o));

    o->stdout_fn = "/dev/null";
    o->stderr_fn = "/dev/null";

    int res;
    while((res = getopt(argc, argv, "Oo:Ee:r:h-")) != -1) {
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
        case 'r':
            o->returncode_fn = strdup(optarg);
            CHECK_NOT(o->returncode_fn, NULL, "strdup(%s)", optarg);
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
    int returncode;

    pid_t stdout_capture;
    pid_t stderr_capture;
} state;

int main(int argc, char* argv[])
{
    memset(&state, 0, sizeof(state));

    struct options o;
    int offset = parse_options(&o, argc, argv);

    for(int i = offset; i < argc; i++) {
        debug("cmdline[%d]=%s", i-offset, argv[i]);
    }

    info("stdout: %s", o.stdout_fn);
    info("stderr: %s", o.stderr_fn);

    sigset_t sm;
    sigemptyset(&sm);
    sigaddset(&sm, SIGINT);
    sigaddset(&sm, SIGQUIT);
    sigaddset(&sm, SIGTERM);
    sigaddset(&sm, SIGCHLD);
    int sfd = signalfd(-1, &sm, SFD_CLOEXEC);
    CHECK(sfd, "signalfd");

    int r = sigprocmask(SIG_BLOCK, &sm, NULL);
    CHECK(r, "sigprocmask");

    // stdout

    int stdout_pair[2];
    r = pipe(stdout_pair); CHECK(r, "pipe");

    state.stdout_capture = fork(); CHECK(state.stdout_capture, "fork");
    if(state.stdout_capture == 0) {
        r = sigprocmask(SIG_UNBLOCK, &sm, NULL);
        CHECK(r, "sigprocmask");

        r = close(stdout_pair[1]); CHECK(r, "close");
        r = dup2(stdout_pair[0], 0); CHECK(r, "dup2(.., 0)");
        r = close(stdout_pair[0]); CHECK(r, "close");

        if(o.silence_stdout) {
            r = close(1); CHECK(r, "close(1)");
            r = open("/dev/null", O_WRONLY);
            CHECK(r, "open(/dev/null, O_WRONLY");
            assert(r == 1);
        }

        r = execlp("tee", "tee", o.stdout_fn, NULL);
        CHECK(r, "execlp(tee, %s)", o.stdout_fn);
    }

    info("spawned stdout capture: %d", state.stdout_capture);
    r = close(stdout_pair[0]); CHECK(r, "close");

    // stderr

    int stderr_pair[2];
    r = pipe(stderr_pair); CHECK(r, "pipe");

    state.stderr_capture = fork(); CHECK(state.stderr_capture, "fork");
    if(state.stderr_capture == 0) {
        r = sigprocmask(SIG_UNBLOCK, &sm, NULL);
        CHECK(r, "sigprocmask");

        r = close(stderr_pair[1]); CHECK(r, "close");
        r = dup2(stderr_pair[0], 0); CHECK(r, "dup2(.., 0)");
        r = close(stderr_pair[0]); CHECK(r, "close");

        if(o.silence_stderr) {
            r = close(1); CHECK(r, "close(1)");
            r = open("/dev/null", O_WRONLY);
            CHECK(r, "open(/dev/null, O_WRONLY");
            assert(r == 1);
        } else {
            r = dup2(2, 1); CHECK(r, "dup2(2,1)");
        }

        r = execlp("tee", "tee", o.stderr_fn, NULL);
        CHECK(r, "execlp(tee, %s)", o.stderr_fn);
    }

    info("spawned stderr capture: %d", state.stderr_capture);
    r = close(stderr_pair[0]); CHECK(r, "close");

    // child

    state.child = fork(); CHECK(state.child, "fork");
    if(state.child == 0) {
        r = sigprocmask(SIG_UNBLOCK, &sm, NULL);
        CHECK(r, "sigprocmask");

        r = dup2(stdout_pair[1], 1); CHECK(r, "dup2(.., 1)");
        r = close(stdout_pair[1]); CHECK(r, "close");
        r = dup2(stderr_pair[1], 2); CHECK(r, "dup2(.., 2)");
        r = close(stderr_pair[1]); CHECK(r, "close");

        // argv[argc] == NULL
        // https://www.gnu.org/software/libc/manual/html_node/Program-Arguments.html
        r = execvp(argv[offset], &argv[offset]);
        CHECK(r, "execv");
    }
    info("spawned child: %d", state.child);

    r = close(0); CHECK(r, "close(0)");
    r = close(stdout_pair[1]); CHECK(r, "close");
    r = close(stderr_pair[1]); CHECK(r, "close");

    while(state.child >= 0
          || state.stdout_capture >= 0 || state.stderr_capture >= 0) {
        struct signalfd_siginfo si;
        ssize_t s = read(sfd, &si, sizeof(si));
        CHECK(s, "read");

        if(si.ssi_signo == SIGCHLD) {
            while(1) {
                int ws;
                pid_t p = waitpid(-1, &ws, WNOHANG);
                if(p == -1 && errno == ECHILD) {
                    break;
                }
                CHECK(p, "waitpid");
                if(p == 0) {
                    break;
                }

                if(WIFEXITED(ws)) {
                    int ec = WEXITSTATUS(ws);
                    if(p == state.child) {
                        info("child (%d) exited: %d", p, ec);
                        state.returncode = ec;
                        state.child = -1;
                    } else if(p == state.stdout_capture) {
                        info("stdout capture (%d) exited: %d", p, ec);
                        state.stdout_capture = -1;
                    } else if(p == state.stderr_capture) {
                        info("stderr capture (%d) exited: %d", p, ec);
                        state.stderr_capture = -1;
                    } else {
                        failwith("unexpected process (%d) exited: %d", p, ec);
                    }
                } else if(WIFSIGNALED(ws)) {
                    int sig = WTERMSIG(ws);
                    const char* sigstr = strsignal(sig);

                    if(p == state.child) {
                        info("child (%d) signaled: %s", p, sigstr);
                        state.returncode = -sig;
                        state.child = -1;
                    } else if(p == state.stdout_capture) {
                        info("stdout capture (%d) signaled: %s", p, sigstr);
                        state.stdout_capture = -1;
                    } else if(p == state.stderr_capture) {
                        info("stderr capture (%d) signaled: %s", p, sigstr);
                        state.stderr_capture = -1;
                    } else {
                        failwith("unexpected process (%d) signaled: %s",
                                 p, sigstr);
                    }
                } else {
                    failwith("unexpected waitpid (%d) status: %d", p, ws);
                }
            }
        } else if(si.ssi_signo == SIGINT
              || si.ssi_signo == SIGQUIT
              || si.ssi_signo == SIGTERM) {
            int sig = si.ssi_signo;
            const char* sigstr = strsignal(sig);
            info("%s", sigstr);
            if(state.child >= 0) {
                info("child (%d) is still running; sending %s",
                     state.child, sigstr);
                r = kill(state.child, sig);
                CHECK(r, "kill(%d, %s)", state.child, sigstr);
            }
        } else {
            failwith("unexpected signal: %s", strsignal(si.ssi_signo));
        }
    }

    info("child returncode: %d", state.returncode);

    if(o.returncode_fn) {
        debug("writing returncode to: %s", o.returncode_fn);
        int fd = open(o.returncode_fn, O_WRONLY|O_EXCL|O_CREAT);
        CHECK(fd, "open(%s)", o.returncode_fn);

        char buf[48];
        ssize_t s = snprintf(LIT(buf), "%d", state.returncode);
        if(s >= sizeof(buf)) {
            failwith("buffer too small");
        }

        size_t i = 0;
        while(i != s) {
            int r = write(fd, &buf[i], s-i);
            CHECK(r, "write(%s)", o.returncode_fn);
            i += r;
        }

        int r = close(fd); CHECK(r, "close");
    }

    return state.returncode != 0;
}
