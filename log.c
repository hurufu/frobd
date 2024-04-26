#include "log.h"
#include <assert.h>
#include <stdio.h>

#ifndef NO_LOGS_ON_STDERR
int g_log_level = LOG_DEBUG;
const char* g_errname;
#endif

void init_log(void) {
#ifdef NO_LOGS_ON_STDERR
    xfclose(stderr);
#endif
}

char* to_printable(const unsigned char* const p, const unsigned char* const pe,
                                  const size_t s, char b[static const s]) {
    // TODO: Add support for regional characters, ie from 0x80 to 0xFF
    // TODO: Rewrite to_printable using libicu
    // TODO: Experiment with https://en.wikipedia.org/wiki/ISO_2047
    unsigned char* o = (unsigned char*)b;
    for (const unsigned char* x = p; x != pe; x++) {

        assert(o < (unsigned char*)b + s);

        const unsigned char c = *x;
        if (c <= 0x20) {
            *o++ = 0xE2;
            *o++ = 0x90;
            *o++ = 0x80 + c;
        } else if (c == 0x7F) {
            *o++ = 0xE2;
            *o++ = 0x90;
            *o++ = 0x80 + 0x31;
        } else if (c & 0x80) {
            *o++ = 0xE2;
            *o++ = 0x90;
            *o++ = 0x36;
        } else {
            *o++ = c;
        }
    }
    *o = 0x00;
    return b;
}
