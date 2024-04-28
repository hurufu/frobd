#pragma once
// FIXME: This file should be renamed to common.h

#include "frob.h"
#include "log.h"

#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>

#define STX "\x02"
#define ETX "\x03"
#define FS "\x1C"
#define US "\x1F"

#define ACK 0x06
#define NAK 0x15

// TODO: Unify COPY and MCOPY
#define MCOPY(Dst, Src) do {\
    const size_t len = strlen(Src);\
    memcpy(Dst, Src, len);\
    Dst += len;\
} while (0)
#define ECOPY(Dst, Src) do {\
    memcpy(Dst, Src, elementsof(Src));\
    Dst += elementsof(Src);\
} while (0)

#define COPY(Dest, Start, End) memcpy((Dest), Start, min(End - Start, (ptrdiff_t)elementsof(Dest)))
#define UNHEX(Dest, Start, End) do {\
    char* dst = Dest;\
    for (const char* src = Start; src < End; src += 2) {\
        const char tmp[] = { src[0], src[1] };\
        *dst++ = unhex(tmp);\
    }\
} while (0)

#define APPLY_TO_ARRAY(X, Macro) ({\
    assert(sizeof (X) != 0);\
    typeof(X[0]) __attribute__((__unused__)) (*should_be_an_array)[NAIVE_ELEMENTSOF(X)] = &X;\
    Macro;\
})
#define NAIVE_ELEMENTSOF(Arr) ( sizeof(Arr) / (sizeof((Arr)[0]) ?: 1) )

#define elementsof(Arr) APPLY_TO_ARRAY(Arr, NAIVE_ELEMENTSOF(Arr))
#define endof(Arr)  ( (Arr) + elementsof(Arr)     )
#define lastof(Arr) ( (Arr) + elementsof(Arr) - 1 )
#define lengthof(Arr) elementsof(Arr)

#define min(A, B) (A < B ? A : B)
#define isempty(S) ((S)[0] == '\0')

// Used to simplify access to environment variables with computed names
#define getenvfx(Buf, Size, Fmt, ...) ({\
    snprintfx(Buf, Size, Fmt, __VA_ARGS__);\
    char* v = getenv(Buf);\
    if (v)\
        v = trim_whitespaces(v);\
    v;\
})

#define xsigtimedwait(...) XCALL(sigtimedwait, __VA_ARGS__)
#define xfprintf(...) XCALL(fprintf, __VA_ARGS__)
#define xfputs(...) XCALL(fputs, __VA_ARGS__)
#define xfflush(...) XCALL(fflush, __VA_ARGS__)
#define xsigprocmask(...) XCALL(sigprocmask, __VA_ARGS__)
#define xsignalfd(...) XCALL(signalfd, __VA_ARGS__)
#define xselect(M, R, W, E, Timeout) XCALL(select, M, R, W, E, Timeout)
#define xread(...) XCALL(read, __VA_ARGS__)
#define xrread(...) XCALL(rread, __VA_ARGS__)
#define xwrite(...) XCALL(write, __VA_ARGS__)
#define xfclose(...) XCALL(fclose, __VA_ARGS__)
#define xclose(...) XCALL(close, __VA_ARGS__)

#ifdef NDEBUG
#   define XCALL(Syscall, ...) syscall_exitf(#Syscall, Syscall(__VA_ARGS__))
#else
#   define XCALL(Syscall, ...) ({\
        const int __ret = syscall_exitf(#Syscall, Syscall(__VA_ARGS__));\
        __ret;\
    })
#define select_preconditions(...)
#define select_postconditions(Ret, M, R, W, E, Timeout) do {\
    if (!Timeout)\
        assert(Ret != 0);\
    if (iop->time_left.tv_sec > 0)\
        assert(t.tv_sec < iop->time_left.tv_sec);\
    else if (iop->time_left.tv_sec == 0)\
        assert(t.tv_sec == 0);\
    } while (0);
#endif
#define NORETURN __attribute__((__noreturn__))

// Input character type, can be changed to something else
typedef uint8_t input_t;


void set_nonblocking(int fd);
bool is_fd_bad(int fd);

byte_t hex2nibble(char h);
byte_t unhex(const char h[static 2]);

// Same as snprintf, but returns buf and aborts on error
unsigned snprintfx(char* buf, size_t s, const char* fmt, ...);
unsigned xsnprintf(char* buf, size_t s, const char* fmt, ...);

// Removes leading and trailing whitespaces in-place. Returns pointer to the first non-whitespace character
char* trim_whitespaces(char* s);

struct frob_frame_fsm_state {
    unsigned char lrc;
    bool not_first;
    int cs;
    unsigned char* p, *pe;
};

// Calculates LRC of bytes between p and pe
input_t calculate_lrc(input_t* p, const input_t* pe);

int frob_frame_process(struct frob_frame_fsm_state*);
int frob_header_extract(const input_t** p, const input_t* pe, struct frob_header*);
int frob_protocol_transition(int*, const enum FrobMessageType);
int frob_body_extract(enum FrobMessageType, const input_t** p, const input_t* pe, union frob_body*);
int frob_extract_additional_attributes(const input_t**, const input_t*, char (* const out)[19][3]);
const char* frob_type_to_string(enum FrobMessageType);
char frob_trx_type_to_code(enum FrobTransactionType);

// Parses message between p and pe. Returns 0 on success, -1 on error
int parse_message(const input_t* p, const input_t* pe, struct frob_msg*);

// Returns number of bytes written to buf or -1 on error
// FIXME: Make this function no exit on error
ssize_t serialize(size_t s, input_t buf[static s], const struct frob_msg* msg);

// Same as read, but returns -1 if file is too big to fit into buf
// TODO: Make this function static
ssize_t eread(int fd, size_t s, input_t buf[static s]);

// Slurps whole file into buf, returns -1 on error or if the file is too big to fit into buf and sets errno accordingly
ssize_t slurp(const char* name, size_t s, input_t buf[static s]);

// Same as slurp, but exits on error
size_t xslurp(const char* name, size_t s, input_t buf[static s]);

// Writes binary data into buf as hex string. Return value same as snprintf,
// ie number of bytes needed to serialize the data or -1 on error
int snprint_hex(size_t sbuf, input_t buf[static sbuf], size_t sbin, const byte_t bin[static sbin]);
// Same as snprint_hex, but exits on error
int xsnprint_hex(size_t sbuf, input_t buf[static sbuf], size_t sbin, const byte_t bin[static sbin]);

NORETURN void exitb(const char* name);

inline static int syscall_exitf(const char* const name, const int ret) {
    if (ret == -1)
        exitb(name);
    return ret;
}

__attribute__((malloc(free), returns_nonnull))
inline static void* xmalloc(const size_t size) {
    void* const ret = malloc(size);
    if (!ret)
        ABORTF("malloc");
    return ret;
}
