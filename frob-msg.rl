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

    action FS_Start {
        file_start = fpc;
    }

    action FS_Commit {
        printf("FILE: %.*s\n", (int)(fpc - file_start), file_start);
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

    structure = stx ((nf|af|hf|bf|sf) >FS_Start @FS_Commit)* etx any @{ puts("Frame"); };
    frame = stx >LRC_Start ((any-etx) @LRC_Byte)* (etx @LRC_Byte) any @LRC_Check;

    main := ((any-stx)* (frame & structure))+;

    write data;
}%%

ssize_t frob_match(const producer_t producer, const consumer_t consumer) {
    ssize_t s;
    int cs;
    unsigned char buf[2 * 1024];
    unsigned char lrc = 0;
    const unsigned char* file_start = NULL;
    %% write init;
    while ((s = producer(&buf)) > 0) {
        const unsigned char* p = buf, * const pe = buf + s;
        %% write exec;
    }
    return s;
}
