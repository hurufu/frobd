#include "utils.h"
#include "log.h"
#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include <ctype.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>

extern inline ssize_t slurpa(int fd, size_t s, input_t buf[static s]);

input_t calculate_lrc(input_t* p, const input_t* const pe) {
    uint8_t lrc = 0;
    assert(p < pe);
    for (; p != pe; p++)
        lrc ^= *p;
    return lrc;
}

byte_t hex2nibble(const char h) {
    switch (h) {
        case 'A' ... 'F': return h - 'A' + 10;
        case 'a' ... 'f': return h - 'a' + 10;
        case '0' ... '9': return h - '0';
    }
    // FIXME: Any better idea what to do with bad hexadecimal number?
    return '\0';
}

byte_t unhex(const char h[static const 2]) {
    return (hex2nibble(h[0]) << 4) | hex2nibble(h[1]);
}

unsigned snprintfx(char* const buf, const size_t s, const char* const fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    const int r = vsnprintf(buf, s, fmt, ap);
    va_end(ap);
    if (r >= 0) {
        if (r < (ssize_t)s)
            return r;
        else
            errno = ENOBUFS;
    }
    EXITF("vsnprintf failed");
}

char* trim_whitespaces(char* const s) {
    char* p = s;
    while (isspace(*p))
        p++;
    for (char* e = p + strlen(p) - 1; e > p && isspace(*e); e--)
        *e = '\0';
    return p;
}

static char class_to_char(const enum FrobMessageType m) {
    switch (m & FROB_MESSAGE_CLASS_MASK) {
        case FROB_T: return 'T';
        case FROB_D: return 'D';
        case FROB_S: return 'S';
        case FROB_P: return 'P';
        case FROB_I: return 'I';
        case FROB_A: return 'A';
        case FROB_K: return 'K';
        case FROB_M: return 'M';
        case FROB_L: return 'L';
        case FROB_B: return 'B';
    }
    assert(false);
    return '?';
}

const char* frob_type_to_string(const enum FrobMessageType m) {
    static char buf[3];
    buf[0] = class_to_char(m);
    buf[1] = (m & FROB_MESSAGE_NUMBER_MASK) + '0';
    buf[2] = '\0';
    return buf;
}

char frob_trx_type_to_code(const enum FrobTransactionType t) {
    switch (t) {
        case FROB_TRANSACTION_TYPE_PAYMENT: return 'S';
        case FROB_TRANSACTION_TYPE_VOID:   return 'C';
    }
    assert(false);
    return '?';
}

int parse_message(const input_t* const p, const input_t* const pe, struct frob_msg* const msg) {
    const input_t* cur = p;
    const char* err;
    int e;
    if ((e = frob_header_extract(&cur, pe, &msg->header)) != 0) {
        err = "Header";
        goto bail;
    }
    if ((e = frob_body_extract(msg->header.type, &cur, pe, &msg->body)) != 0) {
        err = "Body";
        goto bail;
    }
    if ((e = frob_extract_additional_attributes(&cur, pe, &msg->attr)) != 0) {
        err = "Attributes";
        goto bail;
    }
    // Complete message shall be processed, ie cursor shall point to the end of message
    assert(cur == pe);
    return 0;
bail:
    LOGEX("%s parsing failed: %s", err, strerror(e));
    char tmp[4 * (pe - p)];
    LOGDX("\t%s", PRETTV(p, pe, tmp));
    LOGDX("\t%*s", (int)(cur - p), "^");
    return -1;
}

ssize_t rread(const int fd, void* const buf, const size_t count) {
    ssize_t r;
    while ((r = read(fd, buf, count)) < 0 && errno == EINTR)
        continue;
    return r;
}

ssize_t slurp(const int fd, const size_t s, input_t buf[static const s]) {
    ssize_t l;
    ssize_t r = 0;
    do {
        l = rread(fd, buf + r, s - r);
        if (l < 0)
            return assert(errno != EINTR), -1;
        r += l;
    } while (l != 0);
    return r;
}
