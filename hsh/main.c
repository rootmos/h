#include <sys/stat.h>

#define RLIMIT_DEFAULT_CPU (1<<3)
#define RLIMIT_DEFAULT_NOFILE (1<<3)
#define RLIMIT_DEFAULT_STACK (1<<25)
#define RLIMIT_DEFAULT_DATA (1<<25)
#define RLIMIT_DEFAULT_RSS (1<<15)
#define RLIMIT_DEFAULT_AS (1<<30)

#define LIBR_IMPLEMENTATION
#include "r.h"

#include "seccomp.c"
#include "capabilities.c"

#ifndef DEFAULT_SHELL
#define DEFAULT_SHELL "/bin/sh"
#endif

struct options {
    const char* input;
    const char* shell;

    struct rlimit_spec rlimits[RLIMIT_NLIMITS];
};

static void print_usage(int fd, const char* prog)
{
    dprintf(fd, "usage: %s [OPTION]... INPUT\n", prog);
    dprintf(fd, "\n");
    dprintf(fd, "options:\n");
    dprintf(fd, "  -h       print this message\n");
    dprintf(fd, "  -v       print version information\n");
    dprintf(fd, "\n");
    dprintf(fd, "rlimit options:\n");
    dprintf(fd, "  -rRLIMIT=VALUE set RLIMIT to VALUE\n");
    dprintf(fd, "  -R             use inherited rlimits instead of default\n");
}

#include "version.c"

static void parse_options(struct options* o, int argc, char* argv[])
{
    memset(o, 0, sizeof(*o));
    o->shell = DEFAULT_SHELL;

    rlimit_default(o->rlimits, LENGTH(o->rlimits));

    int res;
    while((res = getopt(argc, argv, "hvr:R")) != -1) {
        switch(res) {
        case 'r': {
            int r = rlimit_parse(o->rlimits, LENGTH(o->rlimits), optarg);
            if(r != 0) {
                dprintf(1, "unable to parse rlimit: %s\n", optarg);
                exit(1);
            }
            break;
        }
        case 'R':
            rlimit_inherit(o->rlimits, LENGTH(o->rlimits));
            break;
        case 'v':
            print_version(argv[0]);
            exit(0);
        case 'h':
        default:
            print_usage(res == 'h' ? 1 : 2, argv[0]);
            exit(res == 'h' ? 0 : 1);
        }
    }

    if(optind < argc) {
        o->input = argv[optind];
        debug("input: %s", o->input);

        struct stat st;
        int r = stat(o->input, &st);
        if(r == -1 && errno == ENOENT) {
            dprintf(2, "error; unable to access input file: %s\n", o->input);
            exit(1);
        }
        CHECK(r, "stat(%s)", o->input);
    } else {
        dprintf(2, "error: no input file specified\n");
        print_usage(2, argv[0]);
        exit(1);
    }
}

int main(int argc, char* argv[])
{
    drop_capabilities();
    no_new_privs();

    struct options o;
    parse_options(&o, argc, argv);

    rlimit_apply(o.rlimits, LENGTH(o.rlimits));

    int shell_fd = open(o.shell, __O_PATH);
    CHECK(shell_fd, "open(%s, O_RDONLY)", o.shell);

    int rsfd = landlock_new_ruleset();

    debug("allowing read access: %s", o.input);
    landlock_allow_read(rsfd, o.input);

    debug("allowing execute access: %s", o.shell);
    struct landlock_path_beneath_attr shell_pb = {
        .allowed_access = LANDLOCK_ACCESS_FS_EXECUTE
            | LANDLOCK_ACCESS_FS_READ_FILE,
        .parent_fd = shell_fd,
    };
    int r = landlock_add_rule(rsfd, LANDLOCK_RULE_PATH_BENEATH, &shell_pb, 0);
    CHECK(r, "landlock_add_rule");

#include "landlock.filesc"

    landlock_apply(rsfd);
    r = close(rsfd); CHECK(r, "close");

    seccomp_apply_filter();

    char* shell = strdup(o.shell); CHECK_MALLOC(shell);
    char* input = strdup(o.input); CHECK_MALLOC(input);
    char* args[] = {
        shell,
        "-r",
        input,
        NULL,
    };

    char* env[] = {
        "PATH=/usr/bin",
        NULL,
    };

    r = fexecve(shell_fd, args, env);
    CHECK(r, "fexecve");

    failwith("unreachable");
}
