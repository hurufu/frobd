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
        case FROB_S1:
        case FROB_S2:
        case FROB_P1:
        case FROB_I1:
        case FROB_A1:
        case FROB_A2:
        default:
            return ENOSYS;
    }
}
