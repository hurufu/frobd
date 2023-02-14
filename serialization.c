#include "frob.h"
#include "utils.h"
#include "log.h"
#include <stdio.h>
#include <assert.h>
#include <stdbool.h>

#define _ __attribute__((__unused__)) ignored
#define fs 0x1C
#define us 0x1F
#define stx 0x02
#define etx 0x03

#define serialize_field(S, P, V)\
    _Generic(V,\
            const unsigned int*: serialize_token,\
            enum FrobMessageType: serialize_type,\
            enum FrobDeviceType: serialize_integer,\
            enum FrobTransactionType: serialize_trx_type,\
            enum FrobTransactionStatus: serialize_integer,\
            const char*: serialize_string,\
            const unsigned char*: xsnprint_hex,\
            bool: serialize_integer,\
            unsigned char: serialize_integer,\
            unsigned short: serialize_integer,\
            unsigned long: serialize_integer\
    )(S, P, sizeof V, V)

// Call serializer, advance buffer pointer and adjust free space
#define XCOPY(Serializer, S, P, V) do {\
    const size_t rs = Serializer(S, P, V);\
    if (rs > S) {\
        if (buf - p == -52) \
            LOGDX("HOP %zd", buf - p);\
        goto bail;\
    }\
    P += rs;\
    S -= rs;\
} while (0)

// Copy any field to buffer (separated by a byte pointed by D)
#define DCOPY(S, P, V, D) XCOPY(serialize_field, S, P, V); BCOPY(S, P, D)
// Copy single byte to buffer
#define BCOPY(S, P, D) XCOPY(append_byte, S, P, D)
// Copy any field to buffer (separated by FS byte)
#define FCOPY(S, P, V) DCOPY(S, P, V, fs)
// Copy any subfield to buffer (separated by US byte)
#define UCOPY(S, P, V) DCOPY(S, P, V, us)

static size_t append_byte(const size_t s, input_t p[static const s], const input_t b) {
    if (s >= 1)
        p[0] = b;
    return 1;
}

static size_t serialize_type(const size_t s, input_t p[static const s], size_t _, const enum FrobMessageType t) {
    const char* const tmp = frob_type_to_string(t);
    if (s >= 2) {
        p[0] = tmp[0];
        p[1] = tmp[1];
    }
    return 2;
}

static size_t serialize_trx_type(const size_t s, input_t p[static const s], size_t _, const enum FrobTransactionType t) {
    if (s >= 1)
        p[0] = frob_trx_type_to_code(t);
    return 1;
}

static size_t serialize_string(const size_t s, input_t p[static const s], const size_t l, const char a[static const l]) {
    return (size_t)(stpncpy((char*)p, a, l) - (char*)p);
}

static size_t serialize_integer(const size_t s, input_t p[static const s], size_t _, const int v) {
    char tmp[16];
    const unsigned ret = xsnprintf(tmp, sizeof tmp, "%u", v);
    if (ret <= s)
        memcpy(p, tmp, ret);
    return ret;
}

static size_t serialize_token(const size_t s, input_t p[static const s], size_t _, const unsigned int* const v) {
    char tmp[7];
    const unsigned ret = xsnprintf(tmp, sizeof tmp, "%" PRIXTOKEN, *v);
    if (ret < sizeof tmp)
        memcpy(p, tmp, ret);
    return ret;
}

ssize_t serialize(const size_t s, input_t buf[static const s], const struct frob_msg* const msg) {
    input_t* p = buf; // Cursor
    size_t f = s; // Free space
    const union frob_body* const b = &msg->body;

    const bool is_magic_ok = strcmp(msg->magic, FROB_MAGIC) == 0;
    assert(is_magic_ok);
    if (!is_magic_ok)
        goto bail;

    BCOPY(f, p, stx);
    FCOPY(f, p, &msg->header.token);
    FCOPY(f, p, msg->header.type);
    switch (msg->header.type) {
        case FROB_T1:
        case FROB_T3:
        case FROB_D4:
        case FROB_P1:
        case FROB_A1:
        case FROB_K1:
            break;
        case FROB_T2:
            FCOPY(f, p, b->t2.info.version);
            FCOPY(f, p, b->t2.info.vendor);
            FCOPY(f, p, b->t2.info.device_type);
            FCOPY(f, p, b->t2.info.device_id);
            break;
        case FROB_T4:
            for (size_t i = 0; i < elementsof(b->t4.supported_versions[0]); i++) {
                if (isempty(b->t4.supported_versions[i]))
                    continue;
                UCOPY(f, p, b->t4.supported_versions[i]);
            }
            XCOPY(append_byte, f, p, fs);
            break;
        case FROB_T5:
            FCOPY(f, p, b->t5.selected_version);
            break;
        case FROB_D5:
            FCOPY(f, p, b->d5.printer_cpl);
            FCOPY(f, p, b->d5.printer_cpl2x);
            FCOPY(f, p, b->d5.printer_cpl4x);
            FCOPY(f, p, b->d5.printer_cpln);
            bool tmp;
            tmp = b->d5.printer_h2; FCOPY(f, p, tmp);
            tmp = b->d5.printer_h4; FCOPY(f, p, tmp);
            tmp = b->d5.printer_inv; FCOPY(f, p, tmp);
            FCOPY(f, p, b->d5.printer_max_barcode_length);
            FCOPY(f, p, b->d5.printer_max_qrcode_length);
            FCOPY(f, p, b->d5.printer_max_bitmap_count);
            FCOPY(f, p, b->d5.printer_max_bitmap_width);
            FCOPY(f, p, b->d5.printer_max_bitmap_height);
            FCOPY(f, p, b->d5.printer_aspect_ratio);
            FCOPY(f, p, b->d5.printer_buffer_max_lines);
            FCOPY(f, p, b->d5.display_lc);
            FCOPY(f, p, b->d5.display_cpl);
            UCOPY(f, p, b->d5.key_name.enter);
            UCOPY(f, p, b->d5.key_name.cancel);
            UCOPY(f, p, b->d5.key_name.check);
            UCOPY(f, p, b->d5.key_name.backspace);
            UCOPY(f, p, b->d5.key_name.delete);
            UCOPY(f, p, b->d5.key_name.up);
            UCOPY(f, p, b->d5.key_name.down);
            UCOPY(f, p, b->d5.key_name.left);
            UCOPY(f, p, b->d5.key_name.right);
            BCOPY(f, p, fs);
            FCOPY(f, p, b->d5.device_topo);
            tmp = b->d5.nfc; FCOPY(f, p, tmp);
            tmp = b->d5.ccr; FCOPY(f, p, tmp);
            tmp = b->d5.mcr; FCOPY(f, p, tmp);
            tmp = b->d5.bar; FCOPY(f, p, tmp);
            break;
        case FROB_S1:
            FCOPY(f, p, b->s1.transaction_type);
            FCOPY(f, p, b->s1.ecr_id);
            FCOPY(f, p, b->s1.payment_id);
            FCOPY(f, p, b->s1.amount_gross);
            FCOPY(f, p, b->s1.amount_net);
            FCOPY(f, p, b->s1.vat);
            FCOPY(f, p, b->s1.currency);
            FCOPY(f, p, b->s1.cashback);
            FCOPY(f, p, b->s1.max_cashback);
            break;
        case FROB_S2:
            FCOPY(f, p, b->s2.error);
            FCOPY(f, p, b->s2.card_token);
            FCOPY(f, p, b->s2.mid);
            FCOPY(f, p, b->s2.tid);
            FCOPY(f, p, b->s2.trx_amount);
            FCOPY(f, p, b->s2.cashback);
            FCOPY(f, p, b->s2.payment_method);
            FCOPY(f, p, b->s2.msg);
            break;
        case FROB_I1:
            FCOPY(f, p, b->i1.status);
            FCOPY(f, p, b->i1.msg);
            break;
        case FROB_A2:
            FCOPY(f, p, b->a2.error);
            FCOPY(f, p, b->a2.msg);
            break;
        case FROB_B1:
            FCOPY(f, p, b->b1.info.version);
            FCOPY(f, p, b->b1.info.vendor);
            FCOPY(f, p, b->b1.info.device_type);
            FCOPY(f, p, b->b1.info.device_id);
            break;
        case FROB_B2:
            FCOPY(f, p, b->b2.info.version);
            FCOPY(f, p, b->b2.info.vendor);
            FCOPY(f, p, b->b2.info.device_type);
            FCOPY(f, p, b->b2.info.device_id);
            FCOPY(f, p, b->b2.result);
            FCOPY(f, p, b->b2.modulus);
            FCOPY(f, p, b->b2.exponent);
            break;
        case FROB_B3:
            FCOPY(f, p, b->b3.kek);
            FCOPY(f, p, b->b3.kcv);
            break;
        case FROB_B4:
            FCOPY(f, p, b->b4.result);
            break;
        case FROB_K0:
            FCOPY(f, p, b->k0.result);
            FCOPY(f, p, b->k0.output);
            break;
        default:
            assert(false);
            goto bail;
    }
    for (size_t i = 0; i < elementsof(msg->attr); i++) {
        if (isempty(msg->attr[i]))
            continue;
        UCOPY(f, p, b->t4.supported_versions[i]);
    }
    BCOPY(f, p, etx);
    BCOPY(f, p, calculate_lrc(buf + 1, p));

    // Free space and cursor should stay consistent
    assert(buf + s - f == p);
    // Parseable frame is created
    assert(frob_frame_process(&(struct frob_frame_fsm_state){.p = buf, .pe = p}) == 0);
    // Parseable message was serialized
    assert(parse_message(buf + 1, p - 2, &(struct frob_msg){ .magic = FROB_MAGIC, .header.type = msg->header.type }) == 0);

    return s - f;
bail:
    // Free space and cursor should stay consistent
    assert(buf + s - f == p);

    return buf - p;
}
