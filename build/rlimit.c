#include <sys/resource.h>

enum rlimit_action {
    RLIMIT_ACTION_INHERIT = 0,
    RLIMIT_ACTION_ZERO = 1,
    RLIMIT_ACTION_ABS = 2,
    RLIMIT_ACTION_EQUAL = 3,
};

struct rlimit_spec {
    const char* name;
    int resource;
    enum rlimit_action action;
    unsigned long value;
};

#ifndef RLIMIT_DEFAULT_CPU
#define RLIMIT_DEFAULT_CPU (0)
#endif

#ifndef RLIMIT_DEFAULT_FSIZE
#define RLIMIT_DEFAULT_FSIZE (0)
#endif

#ifndef RLIMIT_DEFAULT_DATA
#define RLIMIT_DEFAULT_DATA (0)
#endif

#ifndef RLIMIT_DEFAULT_STACK
#define RLIMIT_DEFAULT_STACK (0)
#endif

#ifndef RLIMIT_DEFAULT_CORE
#define RLIMIT_DEFAULT_CORE (0)
#endif

#ifndef RLIMIT_DEFAULT_RSS
#define RLIMIT_DEFAULT_RSS (0)
#endif

#ifndef RLIMIT_DEFAULT_NPROC
#define RLIMIT_DEFAULT_NPROC (0)
#endif

#ifndef RLIMIT_DEFAULT_NOFILE
#define RLIMIT_DEFAULT_NOFILE (0),
#endif

#ifndef RLIMIT_DEFAULT_MEMLOCK
#define RLIMIT_DEFAULT_MEMLOCK (0)
#endif

#ifndef RLIMIT_DEFAULT_AS
#define RLIMIT_DEFAULT_AS (0),
#endif

#ifndef RLIMIT_DEFAULT_LOCKS
#define RLIMIT_DEFAULT_LOCKS (0)
#endif

#ifndef RLIMIT_DEFAULT_SIGPENDING
#define RLIMIT_DEFAULT_SIGPENDING (0)
#endif

#ifndef RLIMIT_DEFAULT_MSGQUEUE
#define RLIMIT_DEFAULT_MSGQUEUE (0)
#endif

#ifndef RLIMIT_DEFAULT_NICE
#define RLIMIT_DEFAULT_NICE (0)
#endif

#ifndef RLIMIT_DEFAULT_RTPRIO
#define RLIMIT_DEFAULT_RTPRIO (0)
#endif

#ifndef RLIMIT_DEFAULT_RTTIME
#define RLIMIT_DEFAULT_RTTIME (0)
#endif

#define RLIMIT_SPEC(rl, v) (struct rlimit_spec) { .name = #rl, .resource = RLIMIT_##rl, .action = RLIMIT_ACTION_ABS, .value = v }

void rlimit_default(struct rlimit_spec rlimits[], size_t len)
{
    struct rlimit_spec defaults[] = {
        RLIMIT_SPEC(CPU, RLIMIT_DEFAULT_CPU),
        RLIMIT_SPEC(FSIZE, RLIMIT_DEFAULT_FSIZE),
        RLIMIT_SPEC(DATA, RLIMIT_DEFAULT_DATA),
        RLIMIT_SPEC(STACK, RLIMIT_DEFAULT_STACK),
        RLIMIT_SPEC(CORE, RLIMIT_DEFAULT_CORE),
        RLIMIT_SPEC(RSS, RLIMIT_DEFAULT_RSS),
        RLIMIT_SPEC(NPROC, RLIMIT_DEFAULT_NPROC),
        RLIMIT_SPEC(NOFILE, RLIMIT_DEFAULT_NOFILE),
        RLIMIT_SPEC(MEMLOCK, RLIMIT_DEFAULT_MEMLOCK),
        RLIMIT_SPEC(AS, RLIMIT_DEFAULT_AS),
        RLIMIT_SPEC(LOCKS, RLIMIT_DEFAULT_LOCKS),
        RLIMIT_SPEC(SIGPENDING, RLIMIT_DEFAULT_SIGPENDING),
        RLIMIT_SPEC(MSGQUEUE, RLIMIT_DEFAULT_MSGQUEUE),
        RLIMIT_SPEC(NICE, RLIMIT_DEFAULT_NICE),
        RLIMIT_SPEC(RTPRIO, RLIMIT_DEFAULT_RTPRIO),
        RLIMIT_SPEC(RTTIME, RLIMIT_DEFAULT_RTTIME),
    };

    assert(RLIMIT_NLIMITS == LENGTH(defaults));

    for(int i = 0; i < (int)len; i++) {
        assert(i == defaults[i].resource);
        memcpy(&rlimits[i], &defaults[i], sizeof(struct rlimit_spec));
    }

    if(len < LENGTH(defaults)) {
        debug("not copying all rlimits: %zu < %zu", len, LENGTH(defaults));
    }
}

void rlimit_inherit(struct rlimit_spec rlimits[], size_t len)
{
    for(size_t i = 0; i < len; i++) {
        rlimits[i].action = RLIMIT_ACTION_INHERIT;
    }
}

int rlimit_parse(struct rlimit_spec rlimits[], size_t len, const char* str)
{
    debug("parsing: %s", str);

    size_t L = strlen(str);
    size_t l = 0;
    while(str[l] != '=' && l < L) {
        l += 1;
    }

    if(l == L) {
        info("unable to parse: %s (unexpected end)", str);
        return 1;
    }

    for(size_t i = 0; i < len; i++) {
        if(strlen(rlimits[i].name) != l) {
            continue;
        }

        if(0 == strncasecmp(str, rlimits[i].name, l)) {
            unsigned long v;
            int r = sscanf(str + l + 1, "%lu", &v);
            if(r != 1) {
                info("unable to parse: %s (value not an unsigned int)", str + l + 1);
                return 1;
            }

            info("rlimit %s: %lu", rlimits[i].name, v);
            rlimits[i].action = RLIMIT_ACTION_ABS;
            rlimits[i].value = v;

            return 0;
        }
    }

    info("unable to parse: %s (no such limit found)", str);
    return 1;
}

static void rlimit_apply(const struct rlimit_spec rlimits[], size_t len)
{
    debug("applying rlimits");

    for(size_t i = 0; i < len; i++) {
        struct rlimit rlp;
        int r = getrlimit(rlimits[i].resource, &rlp);
        CHECK(r, "getrlimit(%s)", rlimits[i].name);

        debug("get rlimit %s: soft=%lu hard=%lu",
              rlimits[i].name, rlp.rlim_cur, rlp.rlim_max);

        if(rlimits[i].action == RLIMIT_ACTION_INHERIT) {
            continue;
        }

        switch(rlimits[i].action) {
        case RLIMIT_ACTION_ZERO:
            rlp.rlim_cur = 0;
            rlp.rlim_max = 0;
            break;
        case RLIMIT_ACTION_ABS:
            rlp.rlim_cur = rlimits[i].value;
            rlp.rlim_max = rlimits[i].value;
            break;
        case RLIMIT_ACTION_EQUAL:
            rlp.rlim_max = rlp.rlim_cur;
            break;
        default:
            failwith("unexpected action: %d", rlimits[i].action);
        }

        debug("set rlimit %s: soft=%lu hard=%lu",
              rlimits[i].name, rlp.rlim_cur, rlp.rlim_max);

        r = setrlimit(rlimits[i].resource, &rlp);
        CHECK(r, "setrlimit(%s)", rlimits[i].name);
    }
}
