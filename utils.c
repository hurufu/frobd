#include "utils.h"
#include "log.h"
#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include <ctype.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

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

unsigned xsnprintf(char* const buf, const size_t s, const char* const fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    const int r = vsnprintf(buf, s, fmt, ap);
    va_end(ap);
    if (r < 0)
        EXITF("vsnprintf");
    return r;
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
    EXITF("vsnprintf");
}

char* trim_whitespaces(char* const s) {
    char* p = s;
    while (isspace(*p))
        p++;
    for (char* e = p + strlen(p) - 1; e != p && isspace(*e); e--)
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
    LOGDX("\t%s", PRETTY(VLA(p, pe)));
    LOGDX("\t%*s", (int)(cur - p), "^");
    return -1;
}

// Has similar semantics to read(2) except that it restarts itself if
// it was interrupted by a signal or until file is fully read
// If whole file was read returns positive interger less than s - bytes read.
// if only part of file was read returns s.
// if error occurs returns -1 and sets errno accordingly
static ssize_t rread(const int fd, const size_t s, input_t buf[static const s]) {
    ssize_t l;
    ssize_t r = 0;
    do {
        while ((l = read(fd, buf + r, s - r)) < 0 && errno == EINTR)
            continue;
        if (l < 0)
            return -1;
        r += l;
    } while (l != 0);
    return r;
}

ssize_t eread(const int fd, const size_t s, input_t buf[static const s]) {
    const ssize_t ret = rread(fd, s, buf);
    if (ret < 0 || (ret >= 0 && (size_t)ret < s))
        return ret;
    assert((size_t)ret == s);
    errno = EFBIG;
    return -1;
}

ssize_t slurp(const char* const name, const size_t s, input_t buf[static const s]) {
    const int fd = open(name, O_RDONLY | O_CLOEXEC);
    if (fd < 0)
        return fd;
    const ssize_t ret = eread(fd, s, buf);
    if (ret < 0) {
        if (close(fd) < 0)
            ABORTF("close %s", name);
        return -1;
    }
    return close(fd) < 0 ? -1 : ret;
}

int snprint_hex(const size_t sbuf, input_t buf[static const sbuf], const size_t sbin, const byte_t bin[static const sbin]) {
    if (sbuf >= sbin * 2 + 1) {
        for (size_t i = 0; i < sbin; i++)
            if (snprintf((char*)buf + i * 2, 3, "%02X", bin[i]) != 2)
                return -1;
    }
    return sbin * 2;
}

size_t xslurp(const char* const name, const size_t s, input_t buf[static const s]) {
    const ssize_t ret = slurp(name, s, buf);
    if (ret < 0)
        EXITF("slurp %s", name);
    return ret;
}

int xsnprint_hex(const size_t sbuf, input_t buf[static const sbuf], const size_t sbin, const byte_t bin[static const sbin]) {
    const int ret = snprint_hex(sbuf, buf, sbin, bin);
    if (ret < 0)
        EXITF("snprint_hex");
    return ret;
}
