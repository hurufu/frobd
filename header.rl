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
    switch (get_class(cla) | hex2nibble(sub)) {
        case FROB_T1 & ~FROB_DESTINATION_MASK: return FROB_T1;
        case FROB_T2 & ~FROB_DESTINATION_MASK: return FROB_T2;
        case FROB_T3 & ~FROB_DESTINATION_MASK: return FROB_T3;
        case FROB_T4 & ~FROB_DESTINATION_MASK: return FROB_T4;
        case FROB_T5 & ~FROB_DESTINATION_MASK: return FROB_T5;
        case FROB_D4 & ~FROB_DESTINATION_MASK: return FROB_D4;
        case FROB_D5 & ~FROB_DESTINATION_MASK: return FROB_D5;
        case FROB_S1 & ~FROB_DESTINATION_MASK: return FROB_S1;
        case FROB_S2 & ~FROB_DESTINATION_MASK: return FROB_S2;
        case FROB_P1 & ~FROB_DESTINATION_MASK: return FROB_P1;
        case FROB_I1 & ~FROB_DESTINATION_MASK: return FROB_I1;
        case FROB_A1 & ~FROB_DESTINATION_MASK: return FROB_A1;
        case FROB_A2 & ~FROB_DESTINATION_MASK: return FROB_A2;
        case FROB_D0 & ~FROB_DESTINATION_MASK: return FROB_D0;
        case FROB_D1 & ~FROB_DESTINATION_MASK: return FROB_D1;
        case FROB_D6 & ~FROB_DESTINATION_MASK: return FROB_D6;
        case FROB_D2 & ~FROB_DESTINATION_MASK: return FROB_D2;
        case FROB_D3 & ~FROB_DESTINATION_MASK: return FROB_D3;
        case FROB_D7 & ~FROB_DESTINATION_MASK: return FROB_D7;
        case FROB_D8 & ~FROB_DESTINATION_MASK: return FROB_D8;
        case FROB_D9 & ~FROB_DESTINATION_MASK: return FROB_D9;
        case FROB_DA & ~FROB_DESTINATION_MASK: return FROB_DA;
        case FROB_K0 & ~FROB_DESTINATION_MASK: return FROB_K0;
        case FROB_K1 & ~FROB_DESTINATION_MASK: return FROB_K1;
        case FROB_K2 & ~FROB_DESTINATION_MASK: return FROB_K2;
        case FROB_K3 & ~FROB_DESTINATION_MASK: return FROB_K3;
        case FROB_K4 & ~FROB_DESTINATION_MASK: return FROB_K4;
        case FROB_K5 & ~FROB_DESTINATION_MASK: return FROB_K5;
        case FROB_K6 & ~FROB_DESTINATION_MASK: return FROB_K6;
        case FROB_K7 & ~FROB_DESTINATION_MASK: return FROB_K7;
        case FROB_K8 & ~FROB_DESTINATION_MASK: return FROB_K8;
        case FROB_K9 & ~FROB_DESTINATION_MASK: return FROB_K9;
        case FROB_M1 & ~FROB_DESTINATION_MASK: return FROB_M1;
        case FROB_L1 & ~FROB_DESTINATION_MASK: return FROB_L1;
        case FROB_B1 & ~FROB_DESTINATION_MASK: return FROB_B1;
        case FROB_B2 & ~FROB_DESTINATION_MASK: return FROB_B2;
        case FROB_B3 & ~FROB_DESTINATION_MASK: return FROB_B3;
        case FROB_B4 & ~FROB_DESTINATION_MASK: return FROB_B4;
    }
    return 0;
}

struct frob_header frob_header_extract(const byte_t** px, const byte_t* const pe) {
    const byte_t* p = *px, * const start = p, * token_end = NULL, * type_end = NULL;
    int cs;
    %%{
        write init;
        write exec;
    }%%

#   if 0
    // FIXME: This module somehow ends in error state on valid headers
    if (frob_header_error == cs)
        return (struct frob_header){ };
#   endif

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
