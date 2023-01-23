#include "utils.h"
#include "log.h"
#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include <ctype.h>

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

const char* snprintfx(char* const buf, const size_t s, const char* const fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    const int r = vsnprintf(buf, s, fmt, ap);
    va_end(ap);
    if (r >= 0) {
        if (r < (ssize_t)s)
            return buf;
        else
            errno = ENOBUFS;
    }
    LOGF("vsnprintf failed");
}

char* trim_whitespaces(char* const s) {
    char* p = s;
    while (isspace(*p))
        p++;
    for (char* e = p + strlen(p) - 1; e > p && isspace(*e); e--)
        *e = '\0';
    return p;
}
