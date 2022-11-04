// libr 0.1.0 (21ba7b86b2ad2d66724a501bf6b6f9b2ddcc68fe) (https://github.com/rootmos/libr.git) (2022-11-04T15:34:33+01:00)
// modules: fail logging util

#ifndef LIBR_HEADER
#define LIBR_HEADER

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
#endif // LIBR_HEADER

#ifdef LIBR_IMPLEMENTATION

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
#endif // LIBR_IMPLEMENTATION
