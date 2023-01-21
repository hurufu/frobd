#pragma once

#include <string.h>
#include <assert.h>

#define STX "\x02"
#define ETX "\x03"
#define FS "\x1C"
#define US "\x1F"

/* uint8_t is better than unsigned char to define a byte, because on some
 * platforms (unsigned) char may have more than 8 bit (TI C54xx 16 bit).
 * The problem is purely theoretical, because I don't target MCUs, but still...
 */
typedef uint8_t byte_t;

#define COPY(Dest, Start, End) memcpy((Dest), Start, min(End - Start, elementsof(Dest)))
#define UNHEX(Dest, Start, End) do {\
    char* dst = Dest;\
    for (const char* src = Start; src < End; src += 2) {\
        const char tmp[] = { src[0], src[1] };\
        *dst++ = unhex(tmp);\
    }\
} while (0)

#define APPLY_TO_ARRAY(X, Macro) ({\
    _Static_assert(sizeof (X) != 0);\
    typeof(X[0]) __attribute__((__unused__)) (*should_be_an_array)[NAIVE_ELEMENTSOF(X)] = &X;\
    Macro;\
})
#define NAIVE_ELEMENTSOF(Arr) ( sizeof(Arr) / (sizeof((Arr)[0]) ?: 1) )

#define elementsof(Arr) APPLY_TO_ARRAY(Arr, NAIVE_ELEMENTSOF(Arr))
#define endof(Arr) APPLY_TO_ARRAY(Arr, (Arr) + sizeof (Arr))
#define lastof(Arr) APPLY_TO_ARRAY(Arr, &(Arr)[sizeof (Arr) - 1])

#define min(A, B) (A < B ? A : B)

byte_t hex2nibble(char h);
byte_t unhex(const char h[static 2]);
