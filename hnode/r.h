// libr 0.1.0 (21ba7b86b2ad2d66724a501bf6b6f9b2ddcc68fe) (https://github.com/rootmos/libr.git) (2022-11-04T15:34:14+01:00)
// modules: landlock fail logging util uv rlimit

#ifndef LIBR_HEADER
#define LIBR_HEADER

// libr: landlock.h

#include <linux/types.h>

int landlock_abi_version(void);
int landlock_new_ruleset(void);
void landlock_allow(int rsfd, const char* path, __u64 allowed_access);
void landlock_allow_read(int fd, const char* path);
void landlock_allow_read_write(int rsfd, const char* path);
void landlock_apply(int fd);

// libr: fail.h

#define CHECK(res, format, ...) CHECK_NOT(res, -1, format, ##__VA_ARGS__)

#define CHECK_NOT(res, err, format, ...) \
    CHECK_IF(res == err, format, ##__VA_ARGS__)

#define CHECK_IF(cond, format, ...) do { \
    if(cond) { \
        r_failwith(__extension__ __FUNCTION__, __extension__ __FILE__, \
                   __extension__ __LINE__, 1, \
                   format "\n", ##__VA_ARGS__); \
    } \
} while(0)

#define CHECK_MALLOC(x) CHECK_NOT(x, NULL, "memory allocation failed")

#ifdef SND_LIB_VERSION
#define CHECK_ALSA(err, format, ...) do { \
    if(err < 0) { \
        r_failwith(__extension__ __FUNCTION__, __extension__ __FILE__, \
                   __extension__ __LINE__, 0, \
                   format ": %s\n", ##__VA_ARGS__, snd_strerror(err)); \
    } \
} while(0)
#endif

#ifdef CL_SUCCESS
#define CHECK_OCL(err, format, ...) do { \
    if(err != CL_SUCCESS) { \
        r_failwith(__extension__ __FUNCTION__, __extension__ __FILE__, \
                   __extension__ __LINE__, 0, \
                   format ": %d\n", ##__VA_ARGS__, err); \
    } \
} while(0)
#endif

#ifdef FREETYPE_MAJOR
#define CHECK_FT(err, format, ...) do { \
    if(err != FT_Err_Ok) { \
        r_failwith(__extension__ __FUNCTION__, __extension__ __FILE__, \
                   __extension__ __LINE__, 0, \
                   format ": (%d) %s\n", ##__VA_ARGS__, err, FT_Error_String(err)); \
    } \
} while(0)
#endif

#ifdef VK_HEADER_VERSION
#define CHECK_VULKAN(res, format, ...) do { \
    if(res != VK_SUCCESS) { \
        r_failwith(__extension__ __FUNCTION__, __extension__ __FILE__, \
                   __extension__ __LINE__, 0, \
                   format ": %d\n", ##__VA_ARGS__, res); \
    } \
} while(0)
#endif

#ifdef AVERROR
#define CHECK_AV(res, format, ...) do { \
    if(res < 0) { \
        r_failwith(__extension__ __FUNCTION__, __extension__ __FILE__, \
                   __extension__ __LINE__, 0, \
                   format ": %s\n", ##__VA_ARGS__, av_err2str(res)); \
    } \
} while(0)
#endif

#define failwith(format, ...) \
    r_failwith(__extension__ __FUNCTION__, __extension__ __FILE__, \
               __extension__ __LINE__, 0, format "\n", ##__VA_ARGS__)

#define not_implemented() failwith("not implemented")

void r_failwith(const char* const caller,
                const char* const file,
                const unsigned int line,
                const int include_errno,
                const char* const fmt, ...)
    __attribute__ ((noreturn, format (printf, 5, 6)));

// libr: logging.h

#include <stdarg.h>

#define LOG_QUIET 0
#define LOG_ERROR 1
#define LOG_WARNING 2
#define LOG_INFO 3
#define LOG_DEBUG 4
#define LOG_TRACE 5

#ifndef LOG_LEVEL
#define LOG_LEVEL LOG_INFO
#endif

#define __r_log(level, format, ...) do { \
    r_log(level, __extension__ __FUNCTION__, __extension__ __FILE__, \
          __extension__ __LINE__, format "\n", ##__VA_ARGS__); \
} while(0)

#ifdef __cplusplus
void r_dummy(...);
#else
void r_dummy();
#endif

#if LOG_LEVEL >= LOG_ERROR
#define error(format, ...) __r_log(LOG_ERROR, format, ##__VA_ARGS__)
#else
#define error(format, ...) do { if(0) r_dummy(__VA_ARGS__); } while(0)
#endif

#if LOG_LEVEL >= LOG_WARNING
#define warning(format, ...) __r_log(LOG_WARNING, format, ##__VA_ARGS__)
#else
#define warning(format, ...) do { if(0) r_dummy(__VA_ARGS__); } while(0)
#endif

#if LOG_LEVEL >= LOG_INFO
#define info(format, ...) __r_log(LOG_INFO, format, ##__VA_ARGS__)
#else
#define info(format, ...) do { if(0) r_dummy(__VA_ARGS__); } while(0)
#endif

#if LOG_LEVEL >= LOG_DEBUG
#define debug(format, ...) __r_log(LOG_DEBUG, format, ##__VA_ARGS__)
#else
#define debug(format, ...) do { if(0) r_dummy(__VA_ARGS__); } while(0)
#endif

#if LOG_LEVEL >= LOG_TRACE
#define trace(format, ...) __r_log(LOG_TRACE, format, ##__VA_ARGS__)
#else
#define trace(format, ...) do { if(0) r_dummy(__VA_ARGS__); } while(0)
#endif

void r_log(int level,
           const char* const caller,
           const char* const file,
           const unsigned int line,
           const char* const fmt, ...)
    __attribute__ ((format (printf, 5, 6)));

void r_vlog(int level,
            const char* const caller,
            const char* const file,
            const unsigned int line,
            const char* const fmt, va_list vl);

// libr: util.h

#define LENGTH(xs) (sizeof(xs)/sizeof((xs)[0]))
#define LIT(x) x,sizeof(x)

#ifndef MAX
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#endif

#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif

const char* getenv_mandatory(const char* const env);

// returns current time formated as compact ISO8601: 20190123T182628Z
const char* now_iso8601(void);

void set_blocking(int fd, int blocking);
void no_new_privs(void);

// libr: uv.h

#ifdef UV_VERSION_MAJOR
#define CHECK_UV(err, format, ...) do { \
    if(err != 0) { \
        r_failwith(__extension__ __FUNCTION__, __extension__ __FILE__, \
                   __extension__ __LINE__, 0, \
                   format ": %s\n", ##__VA_ARGS__, uv_err_name(err)); \
    } \
} while(0)
#endif

// libr: rlimit.h

#include <stddef.h>

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

void rlimit_default(struct rlimit_spec rlimits[], size_t len);
void rlimit_inherit(struct rlimit_spec rlimits[], size_t len);

int rlimit_parse(struct rlimit_spec rlimits[], size_t len, const char* str);
void rlimit_apply(const struct rlimit_spec rlimits[], size_t len);
#endif // LIBR_HEADER

#ifdef LIBR_IMPLEMENTATION

// libr: landlock.c

#include <fcntl.h>
#include <unistd.h>
#include <linux/unistd.h>
#include <linux/landlock.h>

#ifndef landlock_create_ruleset
static inline int landlock_create_ruleset(
    const struct landlock_ruleset_attr* const attr,
    const size_t size, const __u32 flags)
{
    return syscall(__NR_landlock_create_ruleset, attr, size, flags);
}
#endif

#ifndef landlock_add_rule
static inline int landlock_add_rule(const int ruleset_fd,
                                    const enum landlock_rule_type rule_type,
                                    const void* const rule_attr,
                                    const __u32 flags)
{
    return syscall(__NR_landlock_add_rule, ruleset_fd, rule_type, rule_attr,
                   flags);
}
#endif

#ifndef landlock_restrict_self
static inline int landlock_restrict_self(const int ruleset_fd,
                                         const __u32 flags)
{
    return syscall(__NR_landlock_restrict_self, ruleset_fd, flags);
}
#endif

int landlock_abi_version(void)
{
    int r = landlock_create_ruleset(
        NULL, 0, LANDLOCK_CREATE_RULESET_VERSION);
    CHECK(r, "landlock ABI version");
    return r;
    if(r < 1) {
        failwith("landlock not supported (ABI=%d)", r);
    }
}

int landlock_new_ruleset(void)
{
    struct landlock_ruleset_attr rs = {
        .handled_access_fs =
              LANDLOCK_ACCESS_FS_EXECUTE
            | LANDLOCK_ACCESS_FS_WRITE_FILE
            | LANDLOCK_ACCESS_FS_READ_FILE
            | LANDLOCK_ACCESS_FS_READ_DIR
            | LANDLOCK_ACCESS_FS_REMOVE_DIR
            | LANDLOCK_ACCESS_FS_REMOVE_FILE
            | LANDLOCK_ACCESS_FS_MAKE_CHAR
            | LANDLOCK_ACCESS_FS_MAKE_DIR
            | LANDLOCK_ACCESS_FS_MAKE_REG
            | LANDLOCK_ACCESS_FS_MAKE_SOCK
            | LANDLOCK_ACCESS_FS_MAKE_FIFO
            | LANDLOCK_ACCESS_FS_MAKE_BLOCK
            | LANDLOCK_ACCESS_FS_MAKE_SYM
            // TODO: | LANDLOCK_ACCESS_FS_REFER,
    };

    int rsfd = landlock_create_ruleset(&rs, sizeof(rs), 0);
    CHECK(rsfd, "landlock_create_ruleset");

    return rsfd;
}

void landlock_allow(int rsfd, const char* path, __u64 allowed_access)
{
    struct landlock_path_beneath_attr pb = {
        .allowed_access = allowed_access,
    };

    pb.parent_fd = open(path, __O_PATH|O_CLOEXEC);
    CHECK(pb.parent_fd, "open(%s, O_PATH)", path);

    int r = landlock_add_rule(rsfd, LANDLOCK_RULE_PATH_BENEATH, &pb, 0);
    CHECK(r, "landlock_add_rule");

    r = close(pb.parent_fd); CHECK(r, "close(%s)", path);
}

void landlock_allow_read(int rsfd, const char* path)
{
    landlock_allow(rsfd, path,
        LANDLOCK_ACCESS_FS_READ_FILE
    );
}

void landlock_allow_read_write(int rsfd, const char* path)
{
    landlock_allow(rsfd, path,
        LANDLOCK_ACCESS_FS_READ_FILE
      | LANDLOCK_ACCESS_FS_WRITE_FILE
      | LANDLOCK_ACCESS_FS_MAKE_REG
      | LANDLOCK_ACCESS_FS_REMOVE_FILE
    );
}

void landlock_apply(int fd)
{
    int r = landlock_restrict_self(fd, 0);
    CHECK(r, "landlock_restrict_self");
}

// libr: fail.c

#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

void r_failwith(const char* const caller,
                const char* const file,
                const unsigned int line,
                const int include_errno,
                const char* const fmt, ...)
{
    va_list vl;
    va_start(vl, fmt);

    if(include_errno) {
        r_log(LOG_ERROR, caller, file, line, "(%s) ", strerror(errno));
        vfprintf(stderr, fmt, vl);
    } else {
        r_vlog(LOG_ERROR, caller, file, line, fmt, vl);
    }
    va_end(vl);

    abort();
}

// libr: logging.c

#include <stdio.h>
#include <unistd.h>

#ifdef __cplusplus
void r_dummy(...)
#else
void r_dummy()
#endif
{
    failwith("called the dummy function, you dummy!");
}

void r_vlog(int level,
            const char* const caller,
            const char* const file,
            const unsigned int line,
            const char* const fmt, va_list vl)
{
    fprintf(stderr, "%s:%d:%s:%s:%u ",
            now_iso8601(), getpid(), caller, file, line);

    vfprintf(stderr, fmt, vl);
}

void r_log(int level,
           const char* const caller,
           const char* const file,
           const unsigned int line,
           const char* const fmt, ...)
{
    va_list vl;
    va_start(vl, fmt);
    r_vlog(level, caller, file, line, fmt, vl);
    va_end(vl);
}

// libr: util.c

#include <assert.h>
#include <fcntl.h>
#include <stdlib.h>
#include <time.h>
#include <sys/prctl.h>

const char* now_iso8601(void)
{
    static char buf[17];
    const time_t t = time(NULL);
    size_t r = strftime(buf, sizeof(buf), "%Y%m%dT%H%M%SZ", gmtime(&t));
    assert(r > 0);
    return buf;
}

const char* getenv_mandatory(const char* const env)
{
    const char* const v = getenv(env);
    if(v == NULL) { failwith("environment variable %s not set", env); }
    return v;
}

void set_blocking(int fd, int blocking)
{
    int fl = fcntl(fd, F_GETFL, 0);
    if(blocking) {
        fl &= ~O_NONBLOCK;
    } else {
        fl |= O_NONBLOCK;
    }

    int r = fcntl(fd, F_SETFL, fl);
    CHECK(r, "fcntl(%d, F_SETFL, %d)", fd, fl);
}

void no_new_privs(void)
{
    int r = prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);
    CHECK(r, "prctl(PR_SET_NO_NEW_PRIVS, 1)");
}

// libr: rlimit.c

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/resource.h>

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

void rlimit_apply(const struct rlimit_spec rlimits[], size_t len)
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
#endif // LIBR_IMPLEMENTATION
