// libr 0.5.2 (20f582ad9ea9fd35f227a0044a599a66ddbd89fc) (https://github.com/rootmos/libr.git) (2025-05-09T09:56:55+02:00)
// modules: fail logging now util char devnull

#ifndef LIBR_HEADER
#define LIBR_HEADER

#define LIBR(x) x
#define PRIVATE __attribute__((visibility("hidden")))
#define PUBLIC __attribute__((visibility("default")))
#define API PRIVATE


// libr: fail.h

#define CHECK(res, format, ...) CHECK_NOT(res, -1, format, ##__VA_ARGS__)

#define CHECK_NOT(res, err, format, ...) \
    CHECK_IF(res == err, format, ##__VA_ARGS__)

#define CHECK_IF(cond, format, ...) do { \
    if(cond) { \
        LIBR(failwith0)(__extension__ __FUNCTION__, __extension__ __FILE__, \
            __extension__ __LINE__, 1, \
            format "\n", ##__VA_ARGS__); \
    } \
} while(0)

#define CHECK_MALLOC(x) CHECK_NOT(x, NULL, "memory allocation failed")
#define CHECK_MMAP(x) CHECK_NOT(x, MAP_FAILED, "memory mapping failed")

#define failwith(format, ...) \
    LIBR(failwith0)(__extension__ __FUNCTION__, __extension__ __FILE__, \
        __extension__ __LINE__, 0, format "\n", ##__VA_ARGS__)

#define not_implemented() do { failwith("not implemented"); } while(0)

void LIBR(failwith0)(
    const char* const caller,
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

extern int LIBR(logger_fd);

#define __r_log(level, format, ...) do { \
    LIBR(logger)(level, __extension__ __FUNCTION__, __extension__ __FILE__, \
          __extension__ __LINE__, format "\n", ##__VA_ARGS__); \
} while(0)

API void LIBR(dummy)(int foo, ...);

#if LOG_LEVEL >= LOG_ERROR
#define error(format, ...) __r_log(LOG_ERROR, format, ##__VA_ARGS__)
#else
#define error(format, ...) do { if(0) LIBR(dummy)(0, ##__VA_ARGS__); } while(0)
#endif

#if LOG_LEVEL >= LOG_WARNING
#define warning(format, ...) __r_log(LOG_WARNING, format, ##__VA_ARGS__)
#else
#define warning(format, ...) do { if(0) LIBR(dummy)(0, ##__VA_ARGS__); } while(0)
#endif

#if LOG_LEVEL >= LOG_INFO
#define info(format, ...) __r_log(LOG_INFO, format, ##__VA_ARGS__)
#else
#define info(format, ...) do { if(0) LIBR(dummy)(0, ##__VA_ARGS__); } while(0)
#endif

#if LOG_LEVEL >= LOG_DEBUG
#define debug(format, ...) __r_log(LOG_DEBUG, format, ##__VA_ARGS__)
#else
#define debug(format, ...) do { if(0) LIBR(dummy)(0, ##__VA_ARGS__); } while(0)
#endif

#if LOG_LEVEL >= LOG_TRACE
#define trace(format, ...) __r_log(LOG_TRACE, format, ##__VA_ARGS__)
#else
#define trace(format, ...) do { if(0) LIBR(dummy)(0, ##__VA_ARGS__); } while(0)
#endif

void LIBR(logger)(
    int level,
    const char* const caller,
    const char* const file,
    const unsigned int line,
    const char* const fmt, ...)
__attribute__ ((format (printf, 5, 6)));

void LIBR(vlogger)(
    int level,
    const char* const caller,
    const char* const file,
    const unsigned int line,
    const char* const fmt,
    va_list vl
);

// libr: now.h

// returns current time formated as compact ISO8601: 20190123T182628Z
const char* LIBR(now_iso8601_compact)(void);

// libr: util.h

#ifndef LENGTH
#define LENGTH(xs) (sizeof(xs)/sizeof((xs)[0]))
#endif

#ifndef LIT
#define LIT(x) x,sizeof(x)
#endif

#ifndef STR
#define STR(x) x,strlen(x)
#endif

#ifndef MAX
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#endif

#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif

// libr: char.h

inline int is_digit(char c)
{
    return '0' <= c && c <= '9';
}

inline int is_alpha(char c)
{
    return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z');
}

inline int is_punctuation(char c)
{
    return c == '.'
        || c == ','
        || c == '-'
        || c == '_'
        || c == '\''
        || c == '"';
}

inline int is_whitespace(char c)
{
    return c == ' '
        || c == '\t';
}

// libr: devnull.h

int LIBR(devnull)(int flags);
void LIBR(devnull2)(int fd, int flags);
#endif // LIBR_HEADER

#ifdef LIBR_IMPLEMENTATION

// libr: fail.c

#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

API void LIBR(failwith0)(
    const char* const caller,
    const char* const file,
    const unsigned int line,
    const int include_errno,
    const char* const fmt, ...)
{
    va_list vl;
    va_start(vl, fmt);

    if(include_errno) {
        LIBR(logger)(LOG_ERROR, caller, file, line, "(%s) ", strerror(errno));
        if(vdprintf(LIBR(logger_fd), fmt, vl) < 0) {
            abort();
        }
    } else {
        LIBR(vlogger)(LOG_ERROR, caller, file, line, fmt, vl);
    }
    va_end(vl);

    abort();
}

// libr: logging.c

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

API void LIBR(dummy)(int foo, ...)
{
    abort();
}

int LIBR(logger_fd) API = 2;

API void LIBR(vlogger)(
    int level,
    const char* const caller,
    const char* const file,
    const unsigned int line,
    const char* const fmt, va_list vl)
{
    int r = dprintf(LIBR(logger_fd), "%s:%d:%s:%s:%u ",
        LIBR(now_iso8601_compact)(), getpid(), caller, file, line);
    if(r < 0) {
        abort();
    }

    r = vdprintf(LIBR(logger_fd), fmt, vl);
    if(r < 0) {
        abort();
    }
}

API void LIBR(logger)(
    int level,
    const char* const caller,
    const char* const file,
    const unsigned int line,
    const char* const fmt, ...)
{
    va_list vl;
    va_start(vl, fmt);
    LIBR(vlogger)(level, caller, file, line, fmt, vl);
    va_end(vl);
}

// libr: now.c

#include <time.h>
#include <stdlib.h>

PRIVATE const char* LIBR(now_iso8601_compact)(void)
{
    static char buf[17];
    const time_t t = time(NULL);
    size_t r = strftime(buf, sizeof(buf), "%Y%m%dT%H%M%SZ", gmtime(&t));
    if(r <= 0) abort();
    return buf;
}

// libr: devnull.c

#include <fcntl.h>
#include <unistd.h>

API int LIBR(devnull)(int flags)
{
    int fd = open("/dev/null", flags);
    CHECK(fd, "open(/dev/null, %d)", flags);
    return fd;
}

API void LIBR(devnull2)(int fd, int flags)
{
    int null = LIBR(devnull)(flags);
    int r = dup2(null, fd); CHECK(r, "dup2(.., %d)", fd);
    r = close(null); CHECK(r, "close");
}
#endif // LIBR_IMPLEMENTATION
