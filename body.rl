#include "frob.h"
#include "utils.h"
#include <errno.h>

%%{
    machine common;
    include frob_common "common.rl";
    variable p *pp;
}%%

static int extract_t1(const byte_t** const pp, const byte_t* const pe, struct frob_t1* const out) {
    int cs;
    %%{
        machine frob_t1;
        include common;

        main := fs;

        write data;
        write init;
        write exec;
    }%%
    return 0;
}

static int extract_t2(const byte_t** const pp, const byte_t* const pe, struct frob_t2* const out) {
    int cs;
    const byte_t* c;
    %%{
        machine frob_t2;
        include common;

        action C {
            c = fpc;
        }
        action Version {
            COPY(out->max_supported_version, c, fpc);
        }
        action Vendor {
            COPY(out->vendor, c, fpc);
        }
        action Type {
            COPY(out->device_type, c, fpc);
        }
        action Id {
            COPY(out->device_id, c, fpc);
        }

        version = a+ >C fs @Version;
        vendor = (a+ >C fs @Vendor) | fs;
        type = (a+ >C fs @Type) | fs;
        id = (a+ >C fs @Id) | fs;

        main := (version vendor type id) |
                (version vendor type) |
                (version vendor) |
                (version);

        write data;
        write init;
        write exec;
    }%%
    return 0;
}

static int extract_t3(const byte_t** const pp, const byte_t* const pe, struct frob_t3* const out) {
    int cs;
    const byte_t* c;

    %%{
        machine frob_t3;
        include common;

        main := fs;

        write data;
        write init;
        write exec;
    }%%

    return cs < %%{ write first_final; }%%;
}

static int extract_t4(const byte_t** const pp, const byte_t* const pe, struct frob_t4* const out) {
    int cs, i = 0;
    const byte_t* c;

    %%{
        machine frob_t4;
        include common;

        action C {
            c = fpc;
        }
        action VersionPP {
            COPY(out->supported_versions[i++], c, fpc);
        }

        version = a+ >C fs @VersionPP;

        main := version*;

        write data;
        write init;
        write exec;
    }%%

    return cs < %%{ write first_final; }%%;
}

static int extract_t5(const byte_t** const pp, const byte_t* const pe, struct frob_t5* const out) {
    int cs;
    const byte_t* c;

    %%{
        machine frob_t5;
        include common;

        action C {
            c = fpc;
        }
        action Version {
            COPY(out->selected_version, c, fpc);
        }

        version = a+ >C fs @Version;

        main := version;

        write data;
        write init;
        write exec;
    }%%

    return cs < %%{ write first_final; }%%;
}

static int extract_s1(const byte_t** const pp, const byte_t* const pe, struct frob_s1* const out) {
    int cs;
    const byte_t* c;

    %%{
        machine frob_s1;
        include common;

        action C {
            c = fpc;
        }
        action TransactionType {
            out->transaction_type = (c[0] == 'S') ? FROB_TRANSACTION_TYPE_PAYMENT : FROB_TRANSACTION_TYPE_VOID;
        }
        action EcrId {
            COPY(out->ecr_id, c, fpc);
        }
        action PaymentId {
            COPY(out->payment_id, c, fpc);
        }
        action AmountGross {
            COPY(out->amount_gross, c, fpc);
        }
        action AmountNet {
            COPY(out->amount_net, c, fpc);
        }
        action Vat {
            COPY(out->vat, c, fpc);
        }
        action Cashback {
            COPY(out->cashback, c, fpc);
        }
        action Currency {
            COPY(out->currency, c, fpc);
        }
        action MaxCashback {
            COPY(out->max_cashback, c, fpc);
        }

        amount = digit+;
        transaction_type = ('S' | 'C') >C fs @TransactionType;
        ecr_id = a+ >C fs @EcrId;
        payment_id = a+ >C fs @PaymentId;
        amount_gross = amount >C fs @AmountGross;
        amount_net = amount >C fs @AmountNet;
        vat = amount >C fs @Vat;
        cashback = amount >C fs @Cashback;
        currency = a{3} >C fs @Currency;
        max_cashback = amount >C fs @MaxCashback;

        required = transaction_type ecr_id payment_id amount_gross amount_net vat currency cashback;

        main := (required) |
                (required max_cashback);

        write data;
        write init;
        write exec;
    }%%

    return cs < %%{ write first_final; }%%;
}

int frob_body_extract(const enum FrobMessageType t,
        const byte_t** const p, const byte_t* const pe,
        union frob_body* const out) {
    switch (t) {
        case FROB_T1: return extract_t1(p, pe, &out->t1);
        case FROB_T2: return extract_t2(p, pe, &out->t2);
        case FROB_T3: return extract_t3(p, pe, &out->t3);
        case FROB_T4: return extract_t4(p, pe, &out->t4);
        case FROB_T5: return extract_t5(p, pe, &out->t5);
        case FROB_D1:
        case FROB_D2:
        case FROB_S1: return extract_s1(p, pe, &out->s1);
        case FROB_S2:
        case FROB_P1:
        case FROB_I1:
        case FROB_A1:
        case FROB_A2:
        default:
            return ENOSYS;
    }
}
