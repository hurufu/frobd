#pragma once
// FIXME: This file should be renamed to common.h

#include "frob.h"

#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>
#include <errno.h>
#include <assert.h>

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

// Input character type, can be changed to something else
typedef uint8_t input_t;

byte_t hex2nibble(char h);
byte_t unhex(const char h[static 2]);

// Same as snprintf, but returns buf and aborts on error
unsigned snprintfx(char* buf, size_t s, const char* fmt, ...);

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
// TODO: Rename to xslurp
size_t slurpx(const char* name, size_t s, input_t buf[static s]);
