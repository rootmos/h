#define _GNU_SOURCE
#include <link.h>
#include <dlfcn.h>
#include <stdio.h>
#include <limits.h>

int main(int argc, char* argv[])
{
    if(argc != 2) {
        return 1;
    }

    const char* lib = argv[1];

    void* h = dlopen(lib, RTLD_LAZY);
    if(h == NULL) {
        dprintf(2, "unable to resolve %s: %s\n", lib, dlerror());
        return 1;
    }

    struct link_map* lm;
    int r = dlinfo(h, RTLD_DI_LINKMAP, &lm);
    if(r != 0) {
        dprintf(2, "unable to fetch linkmap of %s: %s\n", lib, dlerror());
        return 1;
    }

    printf("%s\n", lm->l_name);

    return 0;
}
