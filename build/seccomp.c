#include <linux/seccomp.h>
#include <linux/filter.h>

void seccomp_apply_filter()
{
    struct sock_filter filter[] = {
#include "filter.bpfc"
    };

    struct sock_fprog p = { .len = LENGTH(filter), .filter = filter };
    int r = seccomp(SECCOMP_SET_MODE_FILTER, 0, &p);
    CHECK(r, "seccomp(SECCOMP_SET_MODE_FILTER)");
}
