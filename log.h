#pragma once

#include <err.h>
#include <stddef.h>
#include <stdlib.h>
#include <syslog.h>

// Reimplement using error_at_line(3)

/** Serializes bytes between P and Pe into Buffer in a human-readable form.
 */
#define PRETTY(P, Pe, Buffer) to_printable(P, Pe, elementsof(Buffer), Buffer)

/** Façade for warn/warnx/err/errx functions.
 *
 * Level is one of the LOG_* constants defined in syslog.h Message is passed to
 * Method and is printed only if the current log level is greater or equal to
 * the Level. Prefix is prepended to each message. Prologue and Epilogue are
 * executed only if the message is printed before and after the message
 * respectively. They are useful for example to allocate and free pointers that
 * are used in the message. PostScriptum is executed unconditionally regardless
 * if message was printed or not.
 *
 *                         PostScriptum    ,Prefix,Level  ,Method,Prologue,Epilogue,Message     */
#define LOGDXP(P, ...) LOG(                ,"D"   ,DEBUG  ,warnx ,P       ,        ,##__VA_ARGS__)
#define LOGDX(...)     LOG(                ,"D"   ,DEBUG  ,warnx ,        ,        ,##__VA_ARGS__)
#define LOGD(...)      LOG(                ,"D"   ,DEBUG  ,warn  ,        ,        ,##__VA_ARGS__)
#define LOGIX(...)     LOG(                ,"I"   ,INFO   ,warnx ,        ,        ,##__VA_ARGS__)
#define LOGI(...)      LOG(                ,"I"   ,INFO   ,warn  ,        ,        ,##__VA_ARGS__)
#define LOGWX(...)     LOG(                ,"W"   ,WARNING,warnx ,        ,        ,##__VA_ARGS__)
#define LOGW(...)      LOG(                ,"W"   ,WARNING,warn  ,        ,        ,##__VA_ARGS__)
#define LOGEX(...)     LOG(                ,"E"   ,ERR    ,warnx ,        ,        ,##__VA_ARGS__)
#define LOGE(...)      LOG(                ,"E"   ,ERR    ,warn  ,        ,        ,##__VA_ARGS__)
#define EXITFX(...)    LOG(exit(ERR_UNSPEC),"A"   ,ALERT  ,ERRX  ,        ,        ,##__VA_ARGS__)
#define EXITF(...)     LOG(exit(ERR_UNSPEC),"A"   ,ALERT  ,ERR   ,        ,        ,##__VA_ARGS__)
#define ABORTFX(...)   LOG(abort()         ,"F"   ,EMERG  ,warnx ,        ,        ,##__VA_ARGS__)
#define ABORTF(...)    LOG(abort()         ,"F"   ,EMERG  ,warn  ,        ,        ,##__VA_ARGS__)

#define ERRX(...) errx(ERR_UNSPEC, __VA_ARGS__)
#define ERR(...)  err(ERR_UNSPEC, __VA_ARGS__)

// TODO: Report error code based on actual error
#define ERR_CLI 100
#define ERR_SYSCALL 111
#define ERR_UNSPEC EXIT_FAILURE

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
            Method("\x1f" Prefix "\x1f %32s\x1f% 4d\x1f %24s\x1f " Fmt "\x1e", __FILE__, __LINE__, __func__, ##__VA_ARGS__);\
            Epilogue;\
        }
    extern int g_log_level;
#endif

char* to_printable(const unsigned char* p, const unsigned char* pe, size_t s, char b[static s]);
