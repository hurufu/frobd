#include "frob.h"
#include <stdio.h>

size_t s_i;
unsigned char s_working_copy[255];

%%{
    machine frob_structure;
    alphtype unsigned char;

    action FS_Start {}
    action FS_Commit {}

    us  = 0x1F;
    fs  = 0x1C;
    del = 0x7F;

    n = [0-9];
    a = ^cntrl | del;
    h = xdigit;
    b = [01];
    s = a* us;

    nf = n* fs;
    af = a* fs;
    hf = h* fs;
    bf = b* fs;
    sf = s* fs;

    T1 = zlen;
    T2 = af af? af? af?;
    T3 = zlen;
    T4 = sf;

    main := ((nf|af|hf|bf|sf) >FS_Start @FS_Commit)*;

    write data;
}%%

static ssize_t frob_structure() {
}

%%{
    machine frob_frame;
    alphtype unsigned char;

    action LRC_Start {
        lrc = 0;
    }
    action LRC_Byte {
        lrc ^= fc;
    }
    action LRC_Check {
        puts(lrc == fc ? "ACK" : "NAK");
    }

    stx = 0x02;
    etx = 0x03;
    frame = stx >LRC_Start @Reset ((any-etx) @LRC_Byte @Copy)* (etx @LRC_Byte) any @LRC_Check;

    main := ((any-stx)* frame )+;

    write data;
}%%

int frob_process_ecr_input(int cs, const size_t s, const unsigned char buf[static const s]) {
    unsigned char lrc = 0;
    const unsigned char* p = buf, * const pe = buf + s;
    %%{
        write init;
        write exec;
    }%%
    return cs;
}
