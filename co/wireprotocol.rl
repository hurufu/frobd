#include "comultitask.h"
#include "frob.h"
#include "../log.h"
#include <unistd.h>

%%{
    machine wireformat;
    alphtype char;
    include frob_common "../common.rl";

    action LRC_Init {
        LOGDX("wireformat: LRC Init");
        lrc = 0;
    }
    action LRC_Byte {
        LOGDX("wireformat: LRC Byte");
        lrc ^= fc;
    }
    action LRC_Check {
        if (lrc != fc) {
            LOGDX("wireformat: LRC NOK");
            fbreak;
        } else {
            end =fpc;
            LOGDX("wireformat: LRC OK");
        }
    }
    action Frame_Start {
        LOGDX("wireformat: Frame_Start");
        start = fpc;
    }
    action Send {
        const int out = 0;
        LOGDX("wireformat: Lending frame to % 2d % 2zd %p", out, bytes, buf);
        sus_lend(out, buf, bytes);
    }

    frame = stx @LRC_Init ((any-etx) @LRC_Byte >Frame_Start) ((any-etx) @LRC_Byte )* (etx @LRC_Byte) any @LRC_Check @Send;
    #main := ((any-stx)* frame any)*;
    main := frame;
}%%

%% write data;

int fsm_wireformat(void*) {
    char* start = NULL, * end = NULL;
    (void)wireformat_en_main, (void)wireformat_error, (void)wireformat_first_final;
    char lrc;
    ssize_t bytes;
    char buf[1024] = {};
    volatile int cs;
    char* p = buf, * pe = p;
    %% write init;
    while ((bytes = sus_read(STDIN_FILENO, buf, sizeof buf)) > 0) {
        LOGDX("Received bytes: %*s", (int)bytes, buf);
        pe = buf + bytes;
        %% write exec;
        LOGDX("Bytes processed");
    }
    close(STDIN_FILENO);
    LOGWX("STDIN closed %d entry/error/final %d/%d/%d", cs, wireformat_en_main, wireformat_error, wireformat_first_final);
    return cs == wireformat_error ? -1 : 0;
}
