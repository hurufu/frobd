#include "frob.h"
#include <stddef.h>

%%{
    machine frob_header;
    include frob_common "common.rl";

    action Token {
        token_end = fpc;
    }
    action Type {
        type_end = fpc;
    }

    type_t = 'T' [1-5];
    type_s = 'S' [12];
    type_a = 'A1';
    type_d = 'D' [0-9A];
    type_p = 'P1';
    type_k = 'K' [0-9];
    type_m = 'M1';
    type_l = 'L1';
    type_b = 'B' [1-4];
    type = (type_t | type_s | type_a | type_d | type_p | type_k | type_m | type_l | type_b);

    main := (h{2}){1,3} fs >Token type fs >Type;

    write data;

}%%

static unsigned char hex2nibble(const char h) {
    switch (h) {
        case 'A' ... 'F': return h - 'A' + 10;
        case 'a' ... 'f': return h - 'a' + 10;
        case '0' ... '9': return h - '0';
    }
    // FIXME: Any better idea what to do with bad hexadecimal number?
    return '\0';
}

static unsigned char unhex(const char h[static const 2]) {
    return (hex2nibble(h[0]) << 4) | hex2nibble(h[1]);
}

static int get_class(const byte_t c) {
    switch (c) {
        case 'T': return FROB_T;
        case 'D': return FROB_D;
        case 'S': return FROB_S;
        case 'P': return FROB_P;
        case 'I': return FROB_I;
        case 'A': return FROB_A;
        case 'K': return FROB_K;
        case 'M': return FROB_M;
        case 'L': return FROB_L;
        case 'B': return FROB_B;
    }
    return 0;
}

static enum FrobMessageType deserialize_type(const byte_t cla, const byte_t sub) {
    return get_class(cla) | hex2nibble(sub);
}

struct frob_header frob_header_extract(const byte_t** px, const byte_t* const pe) {
    const byte_t* p = *px, * const start = p, * token_end = NULL, * type_end = NULL;
    int cs;
    %%{
        write init;
        write exec;
    }%%

    if (frob_header_error == cs)
        return (struct frob_header){ };

    struct frob_header res = {
        .type = deserialize_type(type_end[-2], type_end[-1])
    };
    int i = 0;
    for (const byte_t* b = start; b != token_end; b += 2) {
        const char t[2] = { b[0], b[1] };
        res.token[i++] = unhex(t);
    }
    *px = p;
    return res;
}
