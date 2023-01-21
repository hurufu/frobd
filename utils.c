#include "utils.h"

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
