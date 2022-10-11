#define PY_SSIZE_T_CLEAN
#include <Python.h>

#define LIBR_IMPLEMENTATION
#include "r.h"

struct options {
};

static void print_usage(int fd, const char* prog)
{
    dprintf(fd, "usage: %s [OPTION]...\n", prog);
    dprintf(fd, "\n");
    dprintf(fd, "options:\n");
    dprintf(fd, "  -h       print this message\n");
}

static void parse_options(struct options* o, int argc, char* argv[])
{
    memset(o, 0, sizeof(*o));

    int res;
    while((res = getopt(argc, argv, "hlst")) != -1) {
        switch(res) {
        case 'h':
        default:
            print_usage(res == 'h' ? 1 : 2, argv[0]);
            exit(res == 'h' ? 0 : 1);
        }
    }
}

int main(int argc, char* argv[])
{
    no_new_privs();

    struct options o;
    parse_options(&o, argc, argv);

    wchar_t *program = Py_DecodeLocale(argv[0], NULL);
    if (program == NULL) {
        fprintf(stderr, "Fatal error: cannot decode argv[0]\n");
        exit(1);
    }
    Py_SetProgramName(program);  /* optional but recommended */
    Py_Initialize();
    PyRun_SimpleString("from time import time,ctime\n"
                       "print('Today is', ctime(time()))\n");
    if (Py_FinalizeEx() < 0) {
        exit(120);
    }
    PyMem_RawFree(program);

    return 0;
}
