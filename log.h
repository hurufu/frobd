#pragma once

#include <err.h>
#include <stddef.h>
#include <stdlib.h>
#include <syslog.h> // for LOG_* constants

// Serialize bytes between P and Pe into Buffer in a human-readable form
#define PRETTY(P, Pe, Buffer) to_printable(P, Pe, elementsof(Buffer), Buffer)

/** Façade for warn/warnx/err/errx functions.
 *
 * The message is printed only if the current log level is greater or equal to
 * the Level. The Level is one of the LOG_* constants defined in syslog.h
 * Prefix is prepended to each message. Prologue, is executed before the message
 * is printed, and Epilogue is executed after the message is printed. They both
 * are executed only if the message is printed. Arguments are passed to the
 * Method. PostScriptum is executed unconditionally after the message is printed.
 *
 *                         PostScriptum,Prefix,Level  ,Method,Prologue,Epilogue,Arguments   */
#define LOGDXP(P, ...) LOG(            ,"D"   ,DEBUG  ,warnx ,P       ,        ,##__VA_ARGS__)
#define LOGDX(...)     LOG(            ,"D"   ,DEBUG  ,warnx ,        ,        ,##__VA_ARGS__)
#define LOGD(...)      LOG(            ,"D"   ,DEBUG  ,warn  ,        ,        ,##__VA_ARGS__)
#define LOGIX(...)     LOG(            ,"I"   ,INFO   ,warnx ,        ,        ,##__VA_ARGS__)
#define LOGI(...)      LOG(            ,"I"   ,INFO   ,warn  ,        ,        ,##__VA_ARGS__)
#define LOGWX(...)     LOG(            ,"W"   ,WARNING,warnx ,        ,        ,##__VA_ARGS__)
#define LOGW(...)      LOG(            ,"W"   ,WARNING,warn  ,        ,        ,##__VA_ARGS__)
#define LOGEX(...)     LOG(            ,"E"   ,ERR    ,warnx ,        ,        ,##__VA_ARGS__)
#define LOGE(...)      LOG(            ,"E"   ,ERR    ,warn  ,        ,        ,##__VA_ARGS__)
#define EXITFX(...)    LOG(exit(1)     ,"A"   ,ALERT  ,ERRX  ,        ,        ,##__VA_ARGS__)
#define EXITF(...)     LOG(exit(1)     ,"A"   ,ALERT  ,ERR   ,        ,        ,##__VA_ARGS__)
#define ABORTFX(...)   LOG(abort()     ,"F"   ,EMERG  ,warnx ,        ,        ,##__VA_ARGS__)
#define ABORTF(...)    LOG(abort()     ,"F"   ,EMERG  ,warn  ,        ,        ,##__VA_ARGS__)

#define ERRX(...) errx(EXIT_FAILURE, __VA_ARGS__)
#define ERR(...)  err(EXIT_FAILURE, __VA_ARGS__)

#define LOG(PostScriptum, ...) ({\
    LOG_STORY(__VA_ARGS__)\
    PostScriptum;\
})

#ifdef NO_LOGS_ON_STDERR
#   define LOG_STORY(...)
#else
#   define LOG_STORY(Prefix, Level, Method, Prologue, Epilogue, Fmt, ...) \
        if (LOG_##Level <= g_log_level) {\
            Prologue;\
            Method(Prefix " %s:%d\t" Fmt, __FILE__, __LINE__, ##__VA_ARGS__);\
            Epilogue;\
        }
    extern int g_log_level;
#endif

int init_log(void);
char* to_printable(const unsigned char* p, const unsigned char* pe, size_t s, char b[static s]);
