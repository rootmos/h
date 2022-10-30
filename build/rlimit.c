#include <sys/resource.h>

struct rlimit_spec {
    const char* name;
    int resource;
    enum {
        RLIMIT_ACTION_INHERIT = 0,
        RLIMIT_ACTION_ZERO = 1,
        RLIMIT_ACTION_ABS = 2,
        RLIMIT_ACTION_EQUAL = 3,
    } action;
    unsigned long value;
};

#define RLIMIT_SPEC_INHERIT(rl) (struct rlimit_spec) { .name = #rl, .resource = RLIMIT_##rl, .action = RLIMIT_ACTION_INHERIT }
#define RLIMIT_SPEC_ZERO(rl) (struct rlimit_spec) { .name = #rl, .resource = RLIMIT_##rl, .action = RLIMIT_ACTION_ZERO }
#define RLIMIT_SPEC_ABS(rl, v) (struct rlimit_spec) { .name = #rl, .resource = RLIMIT_##rl, .action = RLIMIT_ACTION_ABS, .value = v }
#define RLIMIT_SPEC_EQUAL(rl) (struct rlimit_spec) { .name = #rl, .resource = RLIMIT_##rl, .action = RLIMIT_ACTION_EQUAL }

void rlimit_default(struct rlimit_spec rlimits[], size_t len)
{
    struct rlimit_spec defaults[] = {
        RLIMIT_SPEC_ABS(CPU, 1<<3),
        RLIMIT_SPEC_ABS(FSIZE, 1<<12),
        RLIMIT_SPEC_ABS(DATA, 1<<22),
        RLIMIT_SPEC_ZERO(STACK),
        RLIMIT_SPEC_ZERO(CORE),
        RLIMIT_SPEC_ABS(RSS, 1<<23),
        RLIMIT_SPEC_ZERO(NPROC),
        RLIMIT_SPEC_ABS(NOFILE, 1<<4),
        RLIMIT_SPEC_ZERO(MEMLOCK),
        RLIMIT_SPEC_ABS(AS, 1<<24),
        RLIMIT_SPEC_ZERO(LOCKS),
        RLIMIT_SPEC_ZERO(SIGPENDING),
        RLIMIT_SPEC_ZERO(MSGQUEUE),
        RLIMIT_SPEC_ZERO(NICE),
        RLIMIT_SPEC_ZERO(RTPRIO),
        RLIMIT_SPEC_ZERO(RTTIME),
    };

    assert(RLIMIT_NLIMITS == LENGTH(defaults));

    for(size_t i = 0; i < len; i++) {
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
