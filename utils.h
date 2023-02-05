#pragma once

#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <stddef.h>

#define STX "\x02"
#define ETX "\x03"
#define FS "\x1C"
#define US "\x1F"

#define ACK 0x06
#define NAK 0x15

/* uint8_t is better than unsigned char to define a byte, because on some
 * platforms (unsigned) char may have more than 8 bit (TI C54xx 16 bit).
 * The problem is purely theoretical, because I don't target MCUs, but still...
 */
typedef uint8_t byte_t;

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
    _Static_assert(sizeof (X) != 0, "Empty array?");\
    typeof(X[0]) __attribute__((__unused__)) (*should_be_an_array)[NAIVE_ELEMENTSOF(X)] = &X;\
    Macro;\
})
#define NAIVE_ELEMENTSOF(Arr) ( sizeof(Arr) / (sizeof((Arr)[0]) ?: 1) )

#define elementsof(Arr) APPLY_TO_ARRAY(Arr, NAIVE_ELEMENTSOF(Arr))
#define endof(Arr)  ( (Arr) + elementsof(Arr)     )
#define lastof(Arr) ( (Arr) + elementsof(Arr) - 1 )

#define min(A, B) (A < B ? A : B)

// Used to simplify access to environment variables with computed names
#define getenvfx(Buf, Size, Fmt, ...) ({\
    snprintfx(Buf, Size, Fmt, __VA_ARGS__);\
    char* v = getenv(Buf);\
    if (v)\
        v = trim_whitespaces(v);\
    v;\
})

byte_t hex2nibble(char h);
byte_t unhex(const char h[static 2]);

// Same as snprintf, but returns buf and aborts on error
unsigned snprintfx(char* buf, size_t s, const char* fmt, ...);

// Removes leading and trailing whitespaces in-place. Returns pointer to the first non-whitespace character
char* trim_whitespaces(char* s);
