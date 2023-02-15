#pragma once

#include <err.h>
#include <stddef.h>
#include <stdlib.h>

#define LOGDX(Fmt, ...) LOG_X(LOG_DEBUG, "D", Fmt, ##__VA_ARGS__)
#define LOGIX(Fmt, ...) LOG_X(LOG_INFO, "I", Fmt, ##__VA_ARGS__)
#define LOGWX(Fmt, ...) LOG_X(LOG_WARNING, "W", Fmt, ##__VA_ARGS__)
#define LOGEX(Fmt, ...) LOG_X(LOG_ERROR, "E", Fmt, ##__VA_ARGS__)
#define EXITFX(Fmt, ...) LOGF_(errx, Fmt, ##__VA_ARGS__)
#define EXITF(Fmt, ...) LOGF_(err, Fmt, ##__VA_ARGS__)

#define PRETTY(Arr) ({\
    char buffer[4*sizeof(Arr)];\
    to_printable(Arr, endof(Arr), sizeof buffer, buffer);\
})
#define VLA(P, Pe) ( *((input_t(*)[(Pe) - (P)])&(P)) )

// TODO: Make log levels inline with syslog
enum LogLevel {
    LOG_NONE,
    LOG_FATAL,
    LOG_ERROR,
    LOG_WARNING,
    LOG_INFO,
    LOG_DEBUG
};

int init_log(void);
char* to_printable(const unsigned char* p, const unsigned char* pe, size_t s, char b[static s]);

#ifndef NO_LOGS_ON_STDERR
extern enum LogLevel g_log_level;
#   define LOGW(Fmt, ...) (LOG_WARNING > g_log_level ? NOOP : warn("W %s:%d\t" Fmt, __FILE__, __LINE__, ##__VA_ARGS__))
#   define LOGE(Fmt, ...) (LOG_ERROR   > g_log_level ? NOOP : warn("E %s:%d\t" Fmt, __FILE__, __LINE__, ##__VA_ARGS__))
#   define LOGI(Fmt, ...) (LOG_INFO    > g_log_level ? NOOP : warn("I %s:%d\t" Fmt, __FILE__, __LINE__, ##__VA_ARGS__))
#   define LOG_X(Level, Prefix, Fmt, ...) (Level > g_log_level ? NOOP : warnx(Prefix " %s:%d\t" Fmt, __FILE__, __LINE__, ##__VA_ARGS__))
#   define LOGF_(ErrFunction, Fmt, ...) (LOG_FATAL > g_log_level ? exit(EXIT_FAILURE) : ErrFunction (EXIT_FAILURE, "F %s:%d\t" Fmt, __FILE__, __LINE__, ##__VA_ARGS__))
#   define ABORTF(Fmt, ...) ((LOG_FATAL > g_log_level ? warn("F %s:%d\t" Fmt, __FILE__, __LINE__, ##__VA_ARGS__) : NOOP), abort())
#   define ABORTFX(Fmt, ...) ((LOG_FATAL > g_log_level ? warnx("F %s:%d\t" Fmt, __FILE__, __LINE__, ##__VA_ARGS__) : NOOP), abort())
#else
#   define LOGW(Fmt, ...) NOOP
#   define LOGE(Fmt, ...) NOOP
#   define LOGI(Fmt, ...) NOOP
#   define LOG_X(Level, Prefix, Fmt, ...) NOOP
#   define LOGF_(ErrFunction, Fmt, ...) exit(EXIT_FAILURE)
#   define ABORTF(Fmt, ...) abort()
#   define ABORTFX(Fmt, ...) abort()
#endif

#define NOOP (void)0
