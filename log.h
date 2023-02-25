#pragma once

#include <err.h>
#include <stddef.h>
#include <stdlib.h>
#include <syslog.h>
#include <unistd.h>

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
 * are used in the message. PostScriptum is executed unconditionally after the
 * message is printed.
 *
 *                         PostScriptum      ,Prefix,Level  ,Alt,Prologue,Epilogue,Message     */
#define LOGDXP(P, ...) LOG(                  ,"D"   ,DEBUG  ,X  ,P       ,        ,##__VA_ARGS__)
#define LOGDX(...)     LOG(                  ,"D"   ,DEBUG  ,X  ,        ,        ,##__VA_ARGS__)
#define LOGD(...)      LOG(                  ,"D"   ,DEBUG  ,   ,        ,        ,##__VA_ARGS__)
#define LOGIX(...)     LOG(                  ,"I"   ,INFO   ,X  ,        ,        ,##__VA_ARGS__)
#define LOGI(...)      LOG(                  ,"I"   ,INFO   ,   ,        ,        ,##__VA_ARGS__)
#define LOGWX(...)     LOG(                  ,"W"   ,WARNING,X  ,        ,        ,##__VA_ARGS__)
#define LOGW(...)      LOG(                  ,"W"   ,WARNING,   ,        ,        ,##__VA_ARGS__)
#define LOGEX(...)     LOG(                  ,"E"   ,ERR    ,X  ,        ,        ,##__VA_ARGS__)
#define LOGE(...)      LOG(                  ,"E"   ,ERR    ,   ,        ,        ,##__VA_ARGS__)
#define EXITFX(...)    LOG(exit(EXIT_FAILURE),"A"   ,ALERT  ,X  ,        ,        ,##__VA_ARGS__)
#define EXITF(...)     LOG(exit(EXIT_FAILURE),"A"   ,ALERT  ,   ,        ,        ,##__VA_ARGS__)
#define ABORTFX(...)   LOG(abort()           ,"F"   ,EMERG  ,X  ,        ,        ,##__VA_ARGS__)
#define ABORTF(...)    LOG(abort()           ,"F"   ,EMERG  ,   ,        ,        ,##__VA_ARGS__)

#define WARN(Fmt, ...) WARNX(Fmt ": %m", ##__VA_ARGS__)
#define WARNX(Fmt, ...) write_log(STDERR_FILENO, "frob: " Fmt "\n", ##__VA_ARGS__)

#define LOG(PostScriptum, ...) ({\
    LOG_STORY(__VA_ARGS__)\
    PostScriptum;\
})

#ifdef NO_LOGS_ON_STDERR
#   define LOG_STORY(...)
#else
#   define LOG_STORY(Prefix, Level, Alt, Prologue, Epilogue, Fmt, ...) \
        if (LOG_##Level <= g_log_level) {\
            Prologue;\
            WARN##Alt(Prefix " %s:%d\t" Fmt, __FILE__, __LINE__, ##__VA_ARGS__);\
            Epilogue;\
        }
    extern int g_log_level;
#endif

#ifdef UNIT_TEST
#   define LOG_BUFFER_SIZE 8
#else
#   error
#   define LOG_BUFFER_SIZE (4 * IO_BUF_SIZE)
#endif

int init_log(void);
char* to_printable(const unsigned char* p, const unsigned char* pe, size_t s, char b[static s]);
ssize_t write_log(int fd, const char* fmt, ...);
