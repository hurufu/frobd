#include "frob.h"
#include <stdio.h>


%%{
    machine frob;
    alphtype unsigned char;

    action File {
        puts("FILE");
    }

    action Unit {
        puts("UNIT");
    }

    action LRC_Start {
        lrc = 0;
    }

    action LRC_Byte {
        lrc ^= fc;
    }

    action LRC_Check {
        if (lrc == fc) {
            puts("ACK");
        } else {
            puts("NAK");
        }
    }

    stx = 0x02;
    etx = 0x03;
    us  = 0x1F;
    fs  = 0x1C;
    del = 0x7F;

    n = [0-9];
    a = ^cntrl | del;
    h = xdigit;
    b = [01];
    s = a* us @Unit;

    nf = n* fs @File;
    af = a* fs @File;
    hf = h* fs @File;
    bf = b* fs @File;
    sf = s* fs @File;

    T1 = zlen;
    T2 = af af? af? af?;
    T3 = zlen;
    T4 = sf;

    structure = stx (nf|af|hf|bf|sf)* etx any @{ puts("Frame"); };
    frame = stx >LRC_Start ((any-etx) @LRC_Byte)* (etx @LRC_Byte) any @LRC_Check;

    main := ((any-stx)* (frame & structure))+;

    write data;
}%%

ssize_t frob_match(const producer_t producer, const consumer_t consumer) {
    ssize_t s;
    int cs;
    unsigned char buf[2 * 1024];
    unsigned char lrc;
    %% write init;
    while ((s = producer(&buf)) > 0) {
        const unsigned char* p = buf, * const pe = buf + s;
        %% write exec;
    }
    return s;
}
