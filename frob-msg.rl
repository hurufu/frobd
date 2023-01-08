#include "frob.h"
#include <stdio.h>
#include <stdbool.h>

static size_t s_i;
static unsigned char s_working_copy[255];

static size_t s_t;
static const unsigned char* s_items[100];

struct frob_message {
    char token[6+1];
    char type[2+1];
    union {
        struct frob_t1 {
        } t1;
        struct frob_t2 {
            char version[4+1];
            char vendor[20+1];
            char device_type[20+1];
            char device_id[20+1];
        } t2;
        struct frob_t3 {
        } t3;
        struct frob_t4 {
            char supported_versions[100/4][4+1];
        } t4;
        struct frob_t5 {
            char selected_version[4+1];
        } t5;
        struct frob_d4 {
        } d4;
    };
    char additional_attributes[99][100+1];
};

struct FrobMessageT1 {};
struct FrobMessageT2 {
    const char* version;
    const char* vendor;
    const char* device_type;
    const char* device_id;
};
struct FrobMessageT3 {};

struct FrobMessage {
    const unsigned char* token;
    const unsigned char* type;
    union {
        struct FrobMessageT1 t1;
        struct FrobMessageT2 t2;
        struct FrobMessageT3 t3;
    };
    char** additional_attributes; //???
};

%%{
    machine frob_message;
    alphtype unsigned char;

    
    nul = 0x00;
    us  = 0x1F;
    del = 0x7F;

    n = [0-9];
    a = ascii - cntrl - del;
    h = xdigit;
    b = [01];
    s = a* us;

    af = a* nul;

    T1 := zlen;
    T2 := (af af? af? af?);
    T3 := zlen;

    action Select_T1 {}
    action Select_T2 {}
    action Select_T3 {}
    action Select_T4 {}
    action Select_T5 {}
    action Select_S1 {}
    action Select_S2 {}
    action Select_A1 {}

    type_t1 = 'T1' @Select_T1;
    type_t2 = 'T2' @Select_T2;
    type_t3 = 'T3' @Select_T3;
    type_t4 = 'T4' @Select_T4;
    type_t5 = 'T5' @Select_T5;
    type_s1 = 'S1' @Select_S1;
    type_s2 = 'S2' @Select_S2;
    type_a1 = 'A1' @Select_A1;

    type_t = type_t1 | type_t2 | type_t3 | type_t4 | type_t5;
    type_s = type_s1 | type_s2;
    type_a = type_a1;
    type_d = 'D' ([0-9] | 'A');
    type_p = 'P1';
    type_k = 'K' [0-9];
    type_m = 'M1';
    type_l = 'L1';
    type_b = 'B' [1-4];
    type = (type_t | type_s | type_a | type_d | type_p | type_k | type_m | type_l | type_b) nul;

    main := h* nul type;

    write data;
}%%

static int process_frob_message(void) {
    switch (s_items[1][0] | s_items[1][1] << 8) {
        case 'T' | '1' << 8:
            fprintf(stderr, "!!T1");
            break;
        case 'T' | '2' << 8:
            fprintf(stderr, "!!T2");
            break;
    }
    %%{
        write init;
        write exec;
    }%%
    return 0;
}

%%{
    machine frob_structure;
    alphtype unsigned char;

    action FS_Reset {
        s_t = 0;
    }
    action FS_Start {
        s_items[s_t++] = fpc;
    }
    action FS_End {
        fc = '\0';
    }
    action FS_Commit {
        for (size_t i = 0; i < s_t; i++)
            printf("FIELD: %s\n", s_items[i]);
        if (process_frob_message())
            fbreak;
    }
    action US_End {
    }

    us  = 0x1F;
    fs  = 0x1C;
    del = 0x7F;

    n = [0-9];
    a = ^cntrl | del;
    h = xdigit;
    b = [01];
    s = a* us @US_End;

    nf = n* >FS_Start fs @FS_End;
    af = a* >FS_Start fs @FS_End;
    hf = h* >FS_Start fs @FS_End;
    bf = b* >FS_Start fs @FS_End;
    sf = s* >FS_Start fs @FS_End;

    structure = (nf|af|hf|bf|sf)+;
    main := structure >FS_Reset %FS_Commit;

    write data;
}%%

static int process_frob_structure(void) {
    int cs;
    unsigned char* p = s_working_copy;
    const unsigned char* const pe = s_working_copy + s_i, * const eof = pe;
    %%{
        write init;
        write exec;
    }%%
    return 0;
}
