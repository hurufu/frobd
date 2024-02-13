#include "comultitask.h"
#include "frob.h"
#include <unistd.h>

%%{
    machine wireformat;
    alphtype char;
    include frob_common "../common.rl";

    action LRC_Init { lrc = 0; }
    action LRC_Byte { lrc ^= fc; }
    action LRC_Check {
        if (lrc != fc)
            fbreak;
    }
    action Frame_Start { start = fpc; }
    action Send { sus_borrow(to, NULL); }

    frame = stx @LRC_Init ((any-etx) @LRC_Byte >Frame_Start) ((any-etx) @LRC_Byte )* (etx @LRC_Byte) any @LRC_Check @Send;
    main := ((any-stx)* frame any)*;
}%%

%% write data;

int fsm_wireformat(const struct args_wireformat* const a) {
    char* start = NULL, * end = NULL;
    int fd = 1;
    unsigned lrc;
    ssize_t bytes;
    char buf[1024];
    int cs;
    int to = 4;
    char* p = buf, * pe = p;
    %% write init;
    while ((bytes = sus_read(fd, buf, sizeof buf)) > 0) {
        %% write exec;
    }
}
