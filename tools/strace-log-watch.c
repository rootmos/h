#define _GNU_SOURCE
#include <assert.h>
#include <fcntl.h>
#include <libgen.h>
#include <linux/limits.h>
#include <poll.h>
#include <signal.h>
#include <sys/inotify.h>
#include <sys/signalfd.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>

#define LIBR_IMPLEMENTATION
#include "r.h"

#ifndef pidfd_open
int pidfd_open(pid_t pid, unsigned int flags)
{
    return syscall(__NR_pidfd_open, pid, flags);
}
#endif

struct buffer {
    uint8_t* bs;
    size_t L;
    size_t i;
};

void buffer_init(struct buffer* b, size_t L)
{
    b->bs = calloc(L, 1); CHECK_MALLOC(b->bs);
    b->L = L;
    b->i = 0;
}

void buffer_extend(struct buffer* b)
{
    size_t L = b->L << 1;
    debug("extending buffer: %zu -> %zu", b->L, L);
    uint8_t* bs = calloc(L, 1); CHECK_MALLOC(bs);
    memcpy(bs, b->bs, b->i);
    free(b->bs);
    b->bs = bs;
    b->L = L;
}

void buffer_free(struct buffer* b)
{
    free(b->bs);
    b->bs = NULL;
    b->L = 0;
    b->i = 0;
}

int buffer_full(const struct buffer* b)
{
    return b->i == b->L;
}

size_t buffer_available(const struct buffer* b)
{
    return b->L - b->i;
}

int buffer_empty(const struct buffer* b)
{
    return b->i == 0;
}

ssize_t buffer_find(const struct buffer* b, uint8_t c)
{
    for(size_t i = 0; i < b->i; i++) {
        if(b->bs[i] == c) {
            return i;
        }
    }
    return -1;
}

void buffer_read(struct buffer* b, int fd)
{
    while(1) {
        if(buffer_full(b)) {
            buffer_extend(b);
        }

        const size_t l = b->L - b->i;
        ssize_t s = read(fd, &b->bs[b->i], l);
        if(s == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            return;
        }
        CHECK(s, "read(%d, .., %zu)", fd, l);

        debug("read(%d) = %zd", fd, s);
        if(s == 0) {
            break;
        }

        b->i += s;
    }
}

void buffer_write(struct buffer* b, int fd)
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

void buffer_append(struct buffer* b, void* p, size_t len)
{
    while(buffer_available(b) < len) {
        buffer_extend(b);
    }

    memcpy(&b->bs[b->i], p, len);
    b->i += len;
}

void buffer_move(struct buffer* in, struct buffer* out, size_t len)
{
    if(in->i < len) {
        failwith("asked to move more than available: %zu < %zu", in->i, len);
    }

    while(buffer_available(out) < len) {
        buffer_extend(out);
    }

    memcpy(&out->bs[out->i], in->bs, len);

    out->i += len;

    size_t j = in->i - len;
    memmove(in->bs, &in->bs[len], j);
    in->i = j;
}

static void split_path(const char* path, char** dir, char** base)
{
    char buf0[PATH_MAX];
    strncpy(buf0, path, sizeof(buf0)-1);
    buf0[sizeof(buf0)-1] = '\0';
    *dir = strdup(dirname(buf0)); CHECK_MALLOC(*dir);

    strncpy(buf0, path, sizeof(buf0)-1);
    buf0[sizeof(buf0)-1] = '\0';
    *base = strdup(basename(buf0)); CHECK_MALLOC(*base);
}

struct options {
    pid_t pid;
    const char* pattern;
};

static void print_usage(int fd, const char* prog)
{
    dprintf(fd, "usage: %s [OPTION]... [COMMAND [ARG]...]\n", prog);
    dprintf(fd, "\n");
    dprintf(fd, "options:\n");
    dprintf(fd, "  -o PATTERN  watch for trace files prefixed with PATTERN\n");
    dprintf(fd, "  -p PID      terminate after PID dies\n");
    dprintf(fd, "  -h          print this message\n");
}

static int parse_options(struct options* o, int argc, char* argv[])
{
    memset(o, 0, sizeof(*o));
    o->pid = -1;

    int res;
    while((res = getopt(argc, argv, "p:o:h-")) != -1) {
        switch(res) {
        case 'o': {
            o->pattern = strdup(optarg);
            CHECK_MALLOC(o->pattern);
            break;
        }
        case 'p': {
            int r = sscanf(optarg, "%d", &o->pid);
            if(r != 1) {
                dprintf(2, "error: unable to parse pid: %s\n", optarg);
                exit(1);
            }
            break;
        }
        case '-':
            goto opt_end;
        case 'h':
        default:
            print_usage(res == 'h' ? 1 : 2, argv[0]);
            exit(res == 'h' ? 0 : 1);
        }
    }
opt_end:

    if(o->pattern == NULL) {
        dprintf(2, "error: no pattern specified\n");
        exit(1);
    }

    return optind;
}

struct trace {
    char* path;
    pid_t pid, tail;
    int fd;
    struct buffer buf;
    struct trace* next;
};

struct {
    sigset_t sm;

    pid_t child;
    int child_terminated;

    char* dir;
    char* prefix;

    int running;

    int wd;

    struct buffer out;

    struct pollfd* fds;
    size_t fds_n, fds_N;

    struct trace* traces;
} state;

static size_t trace_n(void)
{
    size_t i = 0;
    struct trace* t = state.traces;
    while(t != NULL) {
        i += 1;
        t = t->next;
    }
    return i;
}

static struct trace* trace_add(void)
{
    struct trace** t = &state.traces;
    while(*t != NULL) t = &(*t)->next;

    struct trace* s = calloc(1, sizeof(struct trace));
    CHECK_MALLOC(s);
    *t = s;
    return s;
}

static void trace_init(struct trace* t, const char* dir, const char* name)
{
    debug("dir=%s name=%s", dir, name);

    char path[PATH_MAX];
    int r = snprintf(LIT(path), "%s/%s", dir, name);
    if(r >= sizeof(path)) {
        failwith("truncated path: %s/%s", dir, name);
    }

    debug("path=%s", path);
    t->path = strdup(path); CHECK_MALLOC(t->path);

    size_t l = strlen(name);
    if(l == 0) {
        failwith("unable to parse pid: empty filename");
    }
    size_t i = l - 1;
    while(1) {
        char c = name[i];
        if(is_digit(c)) {
            if(i == 0) {
                failwith("unable to parse pid: %s (missing '.')", name);
            } else {
                i -= 1;
            }
        } else if(c == '.') {
            break;
        } else {
            failwith("unable to parse pid: %s (expected digit or '.')", name);
        }
    }
    r = sscanf(&name[i], ".%u", &t->pid);
    if(r != 1) {
        failwith("unable to parse pid: %s (unexpected format)", name);
    }
    debug("pid=%d", t->pid);

    buffer_init(&t->buf, 1<<8);

    int pipefd[2];
    r = pipe2(pipefd, O_CLOEXEC | O_NONBLOCK); CHECK(r, "pipe2");

    t->tail = fork(); CHECK(t->tail, "fork");
    if(t->tail == 0) {
        r = sigprocmask(SIG_UNBLOCK, &state.sm, NULL);
        CHECK(r, "sigprocmask");

        devnull2(0, 0);
        r = dup2(pipefd[1], 1); CHECK(r, "dup2(.., 1)");

        r = execlp("tail", "tail", "--lines=+1", "--follow", path, NULL);
        CHECK(r, "execlp(tail -n+1 -f %s)", path);
    }

    info("spawned %d: tail -n+1 -f %s", t->tail, path);

    r = close(pipefd[1]); CHECK(r, "close");
    t->fd = pipefd[0];
}

static void trace_close(struct trace* t)
{
    debug("pid=%d tail=%d", t->pid, t->tail);

    int r = close(t->fd); CHECK(r, "close");
    buffer_free(&t->buf);
}

static void handle_inotifyfd(int ifd)
{
    char buf[sizeof(struct inotify_event) + NAME_MAX + 1];
    ssize_t s = read(ifd, &buf, sizeof(buf));
    if(s == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
        return;
    }
    CHECK(s, "read");
    if(s < sizeof(struct inotify_event)) {
        failwith("unexpected partial read");
    }
    struct inotify_event* e = (struct inotify_event*)buf;
    const size_t l = sizeof(struct inotify_event) + e->len;
    if(s < l) {
        failwith("unexpected read length: %zd != %zu", s, l);
    }

    if(e->wd != state.wd) {
        failwith("unexpected watch descriptor: %d != %d", e->wd, state.wd);
    }

    if(e->len > 0) {
        size_t L = strlen(state.prefix);
        if(strncmp(state.prefix, e->name, L) == 0) {
            info("matched file: %s/%s", state.dir, e->name);

            struct trace* t = trace_add();
            trace_init(t, state.dir, e->name);
        } else {
            debug("ignoring new file: %s/%s", state.dir, e->name);
        }
    }

    handle_inotifyfd(ifd);
}

static void graceful_shutdown(const char* reason, int sig)
{
    debug("initiating graceful shutdown: %s", reason);

    const char* sigstr = strsignal(sig);

    struct trace* t = state.traces;
    while(t) {
        int r = kill(t->tail, sig);
        CHECK(r, "kill(%d, %s)", t->tail, sigstr);
        debug("kill(%d, %s)", t->tail, sigstr);
        t = t->next;
    }

    if(state.child >= 0 && !state.child_terminated) {
        info("child (%d) is still running; sending %s",
             state.child, sigstr);
        int r = kill(state.child, sig);
        CHECK(r, "kill(%d, %s)", state.child, sigstr);
    }

    state.running = 0;
}

static void handle_signalfd(int sfd)
{
    struct signalfd_siginfo si;
    ssize_t s = read(sfd, &si, sizeof(si));
    if(s == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
        return;
    }
    CHECK(s, "read");
    if(s != sizeof(si)) {
        failwith("unexpected partial read");
    }

    if(si.ssi_signo == SIGCHLD) {
        while(1) {
            int ws;
            pid_t r = waitpid(-1, &ws, WNOHANG);
            if(r == -1 && (errno == ECHILD)) {
                break;
            }
            CHECK(r, "wait");
            if(r == 0) {
                break;
            }

            const char* type = "tail";
            if(r == state.child) {
                type = "child";
            }

            if(WIFEXITED(ws)) {
                info("%s (%d) exited: %d", type, r, WEXITSTATUS(ws));
            } else if(WIFSIGNALED(ws)) {
                info("%s (%d) signaled: %s", type, r, strsignal(WTERMSIG(ws)));
            } else {
                failwith("unexpected waitpid (%d) status: %d", r, ws);
            }

            if(r == state.child) {
                state.child_terminated = 1;
                graceful_shutdown("child terminated", SIGINT);
                break;
            }

            struct trace** t = &state.traces;
            int n = 0;
            while(*t) {
                if((*t)->tail == r) {
                    trace_close(*t);
                    n += 1;
                    *t = (*t)->next;
                    break;
                }

                t = &(*t)->next;
            }

            if(n == 0) {
                failwith("unexpected pid: %d", r);
            }
        }
    } else if(si.ssi_signo == SIGINT || si.ssi_signo == SIGTERM) {
        int sig = si.ssi_signo;
        const char* sigstr = strsignal(sig);
        info("%s", sigstr);
        graceful_shutdown(sigstr, sig);
    } else {
        failwith("unexpected signal: %s", strsignal(si.ssi_signo));
    }

    handle_signalfd(sfd);
}

static struct trace* find_trace_from_fd(int fd)
{
    struct trace* p = state.traces;
    while(p) {
        if(p->fd == fd) {
            return p;
        }
        p = p->next;
    }
    failwith("unable to resolve fd: %d", fd);
}

static void handle_tail(int fd)
{
    struct trace* t = find_trace_from_fd(fd);
    buffer_read(&t->buf, fd);

    ssize_t i = buffer_find(&t->buf, '\n');
    if(i >= 0) {
        char buf[48];
        int r = snprintf(LIT(buf), "%d ", t->pid);
        if(r >= sizeof(buf)) {
            failwith("truncated");
        }
        buffer_append(&state.out, buf, r);

        buffer_move(&t->buf, &state.out, i + 1);
    }
}

static void expand_fds(void)
{
    size_t N = state.fds_N << 1;
    debug("expanding fds: %zu -> %zu", state.fds_N, N);

    struct pollfd* p = calloc(N, sizeof(struct pollfd));
    CHECK_MALLOC(p);

    memcpy(p, state.fds, sizeof(struct pollfd)*state.fds_n);

    free(state.fds);

    state.fds = p;
    state.fds_N = N;
}

static void spawn_child(char* argv[])
{
    for(size_t i = 0; argv[i]; i++) {
        debug("child cmdline[%zu]: %s", i, argv[i]);
    }

    state.child = fork(); CHECK(state.child, "fork");
    if(state.child == 0) {
        int r = sigprocmask(SIG_UNBLOCK, &state.sm, NULL);
        CHECK(r, "sigprocmask");

        // argv[argc] == NULL
        // https://www.gnu.org/software/libc/manual/html_node/Program-Arguments.html
        r = execvp(argv[0], argv);
        CHECK(r, "execv");
    }

    info("spawned child: %d", state.child);
}

static int is_running(void)
{
    return state.running ||
        (state.child >= 0 && !state.child_terminated)
        || state.traces != NULL;
}

int main(int argc, char* argv[])
{
    struct options o;
    int offset = parse_options(&o, argc, argv);

    memset(&state, 0, sizeof(state));
    state.child = -1;

    int wfd = -1;
    if(o.pid >= 0) {
        info("waiting for %d", o.pid);
        wfd = pidfd_open(o.pid, O_NONBLOCK); // TODO: PIDFD_NONBLOCK
        CHECK(wfd, "pidfd_open(%d)", o.pid);
    }

    debug("pattern: %s", o.pattern);
    split_path(o.pattern, &state.dir, &state.prefix);
    debug("dir: %s", state.dir);
    debug("prefix: %s", state.prefix);

    buffer_init(&state.out, 1024);

    int ifd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    CHECK(ifd, "inotify_init1");

    state.wd = inotify_add_watch(ifd, state.dir, IN_CREATE);
    CHECK(state.wd, "inotify_add_watch(%s, IN_CREATE)", state.dir);

    sigemptyset(&state.sm);
    sigaddset(&state.sm, SIGINT);
    sigaddset(&state.sm, SIGTERM);
    sigaddset(&state.sm, SIGCHLD);
    int sfd = signalfd(-1, &state.sm, SFD_NONBLOCK | SFD_CLOEXEC);
    CHECK(sfd, "signalfd");

    int r = sigprocmask(SIG_BLOCK, &state.sm, NULL);
    CHECK(r, "sigprocmask");

    if(offset < argc) {
        spawn_child(&argv[offset]);
    }

    state.running = 1;
    state.fds_N = 1<<4;
    size_t M = 3 + (wfd >= 0);
    assert(state.fds_N >= M);
    state.fds = calloc(state.fds_N, sizeof(struct pollfd));
    CHECK_MALLOC(state.fds);
    state.fds_n = M;

    size_t sfd_i = 0;
    assert(sfd_i < M);
    state.fds[sfd_i].fd = sfd;
    state.fds[sfd_i].events = POLLIN;

    size_t ifd_i = 1;
    assert(ifd_i < M);
    state.fds[ifd_i].fd = ifd;
    state.fds[ifd_i].events = POLLIN;

    size_t ofd_i = 2;
    assert(ofd_i < M);
    state.fds[ofd_i].fd = 1;
    state.fds[ofd_i].events = 0;

    ssize_t wfd_i = wfd >= 0 ? 3 : -1;
    assert((wfd < 0) || wfd_i < M);
    if(wfd_i) {
        state.fds[wfd_i].fd = wfd;
        state.fds[wfd_i].events = POLLIN;
    }

    while(is_running()) {
        if(buffer_empty(&state.out)) {
            state.fds[ofd_i].events = 0;
        } else {
            state.fds[ofd_i].events = POLLOUT;
        }

        const size_t n = trace_n();
        while(n + M > state.fds_N) {
            expand_fds();
        }

        struct trace* t = state.traces;
        size_t i = M;
        while(t != NULL) {
            state.fds[i].fd = t->fd;
            state.fds[i].events = POLLIN;

            i += 1;
            t = t->next;
        }

        debug("polling: %zu", i);
        int r = poll(state.fds, i, -1);
        CHECK(r, "poll");
        debug("poll: %d", r);

        for(size_t j = M; j < i; j++) {
            handle_tail(state.fds[j].fd);
            state.fds[j].revents ^= POLLIN;
        }

        if(state.fds[sfd_i].revents & POLLIN) {
            handle_signalfd(state.fds[sfd_i].fd);
            state.fds[sfd_i].revents ^= POLLIN;
        }

        if(state.fds[ifd_i].revents & POLLIN) {
            handle_inotifyfd(state.fds[ifd_i].fd);
            state.fds[ifd_i].revents ^= POLLIN;
        }

        if(state.fds[ofd_i].revents & POLLOUT) {
            buffer_write(&state.out, state.fds[ofd_i].fd);
            state.fds[ofd_i].revents ^= POLLOUT;
        }

        if(wfd_i && state.fds[wfd_i].revents & POLLIN) {
            graceful_shutdown("waited for pid died", SIGINT);
            state.fds[wfd_i].revents ^= POLLIN;
        }

        for(size_t i = 0; i < state.fds_n; i++) {
            if(state.fds[i].revents != 0) {
                failwith("unhandled events: fds[%zu].revents = %d",
                         i, state.fds[i].revents);
            }
        }
    }

    debug("bye");

    return 0;
}
