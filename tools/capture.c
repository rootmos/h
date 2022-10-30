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

int main(int argc, char* argv[])
{
    struct options o;
    int offset = parse_options(&o, argc, argv);

    for(int i = offset; i < argc; i++) {
        debug("cmdline[%d]=%s", i-offset, argv[i]);
    }
}
