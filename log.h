#pragma once

#include <err.h>
#include <stddef.h>

#define LOGDX(Fmt, ...) LOG_X(LOG_DEBUG, "D", Fmt, ##__VA_ARGS__)
#define LOGIX(Fmt, ...) LOG_X(LOG_INFO, "I", Fmt, ##__VA_ARGS__)
#define LOGWX(Fmt, ...) LOG_X(LOG_WARNING, "W", Fmt, ##__VA_ARGS__)
#define LOGEX(Fmt, ...) LOG_X(LOG_ERROR, "E", Fmt, ##__VA_ARGS__)
#define LOGFX(Fmt, ...) LOGF_(errx, Fmt, ##__VA_ARGS__)
#define LOGF(Fmt, ...) LOGF_(err, Fmt, ##__VA_ARGS__)

#define PRETTY(Arr) to_printable(Arr, endof(Arr), 4*sizeof(Arr), (char[4*sizeof(Arr)]){})
#define PRETTV(P, Pe, Buf) to_printable(P, Pe, sizeof(Buf), Buf)

enum LogLevel {
    LOG_NONE,
    LOG_FATAL,
    LOG_ERROR,
    LOG_WARNING,
    LOG_INFO,
    LOG_DEBUG
};

char* to_printable(const unsigned char* p, const unsigned char* pe, size_t s, char b[static s]);

#ifndef NO_LOGS_ON_STDERR
extern enum LogLevel g_log_level;
#   define LOGW(Fmt, ...) (LOG_WARNING > g_log_level ? 0 : warn("W %s:%d\t" Fmt, __FILE__, __LINE__, ##__VA_ARGS__), -1)
#   define LOG_X(Level, Prefix, Fmt, ...) (Level > g_log_level ? 0 : warnx(Prefix " %s:%d\t" Fmt, __FILE__, __LINE__, ##__VA_ARGS__), -1)
#   define LOGF_(ErrFunction, Fmt, ...) (LOG_FATAL > g_log_level ? 0 : ErrFunction (EXIT_FAILURE, "F %s:%d\t" Fmt, __FILE__, __LINE__, ##__VA_ARGS__), -1)
#else
#   define LOGW(Fmt, ...)
#   define LOG_X(Level, Prefix, Fmt, ...)
#   define LOGF_(ErrFunction, Fmt, ...) exit(EXIT_FAILURE)
#endif
