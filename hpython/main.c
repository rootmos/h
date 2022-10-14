#include <linux/seccomp.h>
#include <linux/filter.h>

#define PY_SSIZE_T_CLEAN
#include <Python.h>

#define LIBR_IMPLEMENTATION
#include "r.h"

void seccomp_apply_filter()
{
    struct sock_filter filter[] = {
#include "filter.bpfc"
    };

    struct sock_fprog p = { .len = LENGTH(filter), .filter = filter };
    int r = prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &p);
    CHECK(r, "prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER)");
}

struct options {
    const char* input;
};

static void print_usage(int fd, const char* prog)
{
    dprintf(fd, "usage: %s [OPTION]... INPUT\n", prog);
    dprintf(fd, "\n");
    dprintf(fd, "options:\n");
    dprintf(fd, "  -h       print this message\n");
}

static void parse_options(struct options* o, int argc, char* argv[])
{
    memset(o, 0, sizeof(*o));

    int res;
    while((res = getopt(argc, argv, "h")) != -1) {
        switch(res) {
        case 'h':
        default:
            print_usage(res == 'h' ? 1 : 2, argv[0]);
            exit(res == 'h' ? 0 : 1);
        }
    }

    if(optind < argc) {
        o->input = argv[optind];
    } else {
        dprintf(2, "error: no input file specified\n");
        print_usage(2, argv[0]);
        exit(1);
    }

    debug("input: %s", o->input);
}

int main(int argc, char* argv[])
{
    no_new_privs();

    struct options o;
    parse_options(&o, argc, argv);

    int rsfd = landlock_new_ruleset();
    landlock_allow_read(rsfd, o.input);
    landlock_allow(rsfd, "/usr/lib/libz.so.1",
        LANDLOCK_ACCESS_FS_READ_FILE);
    landlock_allow(rsfd, "/usr/lib/python3.10", // TODO: how to retrieve this properly?
        LANDLOCK_ACCESS_FS_READ_FILE | LANDLOCK_ACCESS_FS_READ_DIR );

    landlock_apply(rsfd);
    int r = close(rsfd); CHECK(r, "close");

    seccomp_apply_filter();

    wchar_t *pgr = Py_DecodeLocale(argv[0], NULL);
    CHECK_NOT(pgr, NULL, "Py_DecodeLocale(%s)", argv[0]);
    Py_SetProgramName(pgr);

    // TODO: error handling
    PyPreConfig preconfig;
    PyPreConfig_InitIsolatedConfig(&preconfig);
    Py_PreInitialize(&preconfig);

    PyConfig config;
    PyConfig_InitIsolatedConfig(&config);
    Py_InitializeFromConfig(&config);
    PyConfig_Clear(&config);

    debug("opening input file: %s", o.input);
    FILE* f = fopen(o.input, "r");
    CHECK_NOT(f, NULL, "fopen(%s, r)", o.input);

    debug("running file: %s", o.input);
    r = PyRun_SimpleFileExFlags(f, o.input, /*closeit*/ 1, NULL);
    CHECK_NOT(r, -1, "PyRun_SimpleFileExFlags(%s)", o.input);

    Py_FinalizeEx();
    CHECK_NOT(r, -1, "Py_FinalizeEx()");

    PyMem_RawFree(pgr);

    return 0;
}
