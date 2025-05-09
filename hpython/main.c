#include <sys/stat.h>

#define PY_SSIZE_T_CLEAN
#include <Python.h>

#define RLIMIT_DEFAULT_CPU (1<<2)
#define RLIMIT_DEFAULT_DATA (1<<23)
#define RLIMIT_DEFAULT_NOFILE (1<<4)
#define RLIMIT_DEFAULT_RSS (1<<23)
#define RLIMIT_DEFAULT_AS (1<<24)

#define LIBR_IMPLEMENTATION
#include "r.h"

#include "seccomp.c"
#include "capabilities.c"

struct options {
    const char* input;

    struct rlimit_spec rlimits[RLIMIT_NLIMITS];
};

static void print_usage(int fd, const char* prog)
{
    dprintf(fd, "usage: %s [OPTION]... INPUT\n", prog);
    dprintf(fd, "\n");
    dprintf(fd, "options:\n");
    dprintf(fd, "  -h       print this message\n");
    dprintf(fd, "\n");
    dprintf(fd, "rlimit options:\n");
    dprintf(fd, "  -rRLIMIT=VALUE set RLIMIT to VALUE\n");
    dprintf(fd, "  -R             use inherited rlimits instead of default\n");
}

#include "version.c"

static void parse_options(struct options* o, int argc, char* argv[])
{
    memset(o, 0, sizeof(*o));

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

    debug("input: %s", o->input);
}

#define CHECK_PYTHON(status, format, ...) do { \
    if(PyStatus_Exception(status)) { \
        LIBR(failwith0)(__extension__ __FUNCTION__, __extension__ __FILE__, \
                        __extension__ __LINE__, 0, \
                        format " (%s: %s)\n", ##__VA_ARGS__, \
                        status.func, status.err_msg); \
    } \
} while(0)


int main(int argc, char* argv[])
{
    drop_capabilities();
    no_new_privs();

    struct options o;
    parse_options(&o, argc, argv);

    rlimit_apply(o.rlimits, LENGTH(o.rlimits));

    int rsfd = landlock_new_ruleset();
    landlock_allow_read(rsfd, o.input);
#include "landlock.filesc"

    landlock_apply(rsfd);
    int r = close(rsfd); CHECK(r, "close");

    seccomp_apply_filter();

    PyPreConfig preconfig;
    PyPreConfig_InitIsolatedConfig(&preconfig);
    PyStatus s = Py_PreInitialize(&preconfig);
    CHECK_PYTHON(s, "Py_PreInitialize");

    PyConfig config;
    PyConfig_InitIsolatedConfig(&config);

    config.program_name = Py_DecodeLocale(argv[0], NULL);
    CHECK_NOT(config.program_name, NULL, "Py_DecodeLocale(%s)", argv[0]);

    s = Py_InitializeFromConfig(&config);
    CHECK_PYTHON(s, "Py_InitializeFromConfig");
    PyConfig_Clear(&config);
    PyMem_RawFree(config.program_name);

    debug("opening input file: %s", o.input);
    FILE* f = fopen(o.input, "r");
    CHECK_NOT(f, NULL, "fopen(%s, r)", o.input);

    debug("running file: %s", o.input);
    r = PyRun_SimpleFileExFlags(f, o.input, /*closeit*/ 1, NULL);
    if(r == -1) {
        debug("PyRun_SimpleFileExFlags(%s) == -1", o.input);
        exit(2);
    } else {
        CHECK_NOT(r, -1, "PyRun_SimpleFileExFlags(%s)", o.input);
    }

    Py_FinalizeEx();
    CHECK_NOT(r, -1, "Py_FinalizeEx()");

    return 0;
}
