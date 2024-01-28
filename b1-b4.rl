static int extract_b1(const byte_t** const pp, const byte_t* const pe, struct frob_b1* const out) {
    int cs;
    const byte_t* c;

    %%{
        machine frob_b1;
        include common_b;

        main := version vendor type id;

        write data;
        write init;
        write exec;
    }%%

    return cs < %%{ write first_final; }%% ? ENOTRECOVERABLE : 0;
}

static int extract_b2(const byte_t** const pp, const byte_t* const pe, struct frob_b2* const out) {
    int cs;
    const byte_t* c;

    %%{
        machine frob_b2;
        include common_b;

        action Result {
            out->result = STRTOL(c);
        }
        action Modulus {
            UNHEX(out->modulus, c, fpc);
        }
        action Exponent {
            UNHEX(out->exponent, c, fpc);
        }

        result = n* >C fs @Result;
        modulus = h* >C fs @Modulus;
        exponent = h* >C fs @Exponent;
        required = version vendor type id result;

        main := required |
                (required modulus) |
                (required modulus exponent);

        write data;
        write init;
        write exec;
    }%%

    return cs < %%{ write first_final; }%% ? ENOTRECOVERABLE : 0;
}

static int extract_b3(const byte_t** const pp, const byte_t* const pe, struct frob_b3* const out) {
    int cs;
    const byte_t* c;

    %%{
        machine frob_b3;
        include common;

        action C {
            c = fpc;
        }
        action Kek {
            UNHEX(out->kek, c, fpc);
        }
        action Kcv {
            UNHEX(out->kcv, c, fpc);
        }

        kek = h* >C fs @Kek;
        kcv = h* >C fs @Kcv;

        main := kek kcv;

        write data;
        write init;
        write exec;
    }%%

    return cs < %%{ write first_final; }%% ? ENOTRECOVERABLE : 0;
}

static int extract_b4(const byte_t** const pp, const byte_t* const pe, struct frob_b4* const out) {
    int cs;
    const byte_t* c;

    %%{
        machine frob_b4;
        include common;

        action C {
            c = fpc;
        }
        action Result {
            out->result = STRTOL(c);
        }

        result = n* >C fs @Result;

        main := result;

        write data;
        write init;
        write exec;
    }%%

    return cs < %%{ write first_final; }%% ? ENOTRECOVERABLE : 0;
}
