#include <linux/seccomp.h>
#include <linux/filter.h>
#include <sys/prctl.h>

void seccomp_apply_filter()
{
    struct sock_filter filter[] = {
#include "filter.bpfc"
    };

    struct sock_fprog p = { .len = LENGTH(filter), .filter = filter };
    int r = prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &p);
    CHECK(r, "prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER)");
}
