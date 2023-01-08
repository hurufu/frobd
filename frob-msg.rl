#include "frob.h"
#include <stdio.h>
#include <stdbool.h>

static size_t s_i;
static unsigned char s_working_copy[255];

static size_t s_t;
static const unsigned char* s_items[100];

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
        for (size_t i = 0; i < s_t; i++) {
            printf("FIELD: %s\n", s_items[i]);
        }
    }

    us  = 0x1F;
    fs  = 0x1C;
    del = 0x7F;

    n = [0-9];
    a = ^cntrl | del;
    h = xdigit;
    b = [01];
    s = a* us;

    nf = n* >FS_Start fs @FS_End;
    af = a* >FS_Start fs @FS_End;
    hf = h* >FS_Start fs @FS_End;
    bf = b* >FS_Start fs @FS_End;
    sf = s* >FS_Start fs @FS_End;

    T1 = zlen;
    T2 = af af? af? af?;
    T3 = zlen;
    T4 = sf;

    structure = (nf|af|hf|bf|sf)+;
    main := structure >FS_Reset %FS_Commit;

    write data;
}%%

static int process_frob_structure(void) {
    int cs;
    unsigned char* p = s_working_copy, * const pe = s_working_copy + s_i, * const eof = pe;
    %%{
        write init;
        write exec;
    }%%
    return 0;
}

%%{
    machine frob_frame;
    alphtype unsigned char;

    action Copy {
        if (sizeof s_working_copy <= s_i) {
            fbreak;
        }
        s_working_copy[s_i++] = fc;
    }
    action Reset {
        s_i = 0;
    }
    action LRC_Start {
        lrc = 0;
    }
    action LRC_Byte {
        lrc ^= fc;
    }
    action LRC_Check_And_Commit {
        const bool ok = lrc == fc;
        puts(ok ? "ACK" : "NAK");
        if (ok && process_frob_structure()) {
            fbreak;
        }
    }

    stx = 0x02;
    etx = 0x03;
    frame = stx >LRC_Start @Reset ((any-etx) @LRC_Byte @Copy)* (etx @LRC_Byte) any @LRC_Check_And_Commit;

    main := ((any-stx)* frame )+;

    write data;
}%%

int frob_process_ecr_eft_input(const size_t s, const unsigned char buf[static const s]) {
    static int cs = frob_frame_start;
    static unsigned char lrc = 0;

    const unsigned char* p = buf, * const pe = buf + s;
    %% write exec;
    return cs;
}
