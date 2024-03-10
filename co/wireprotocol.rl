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
            LOGDX("wireformat: LRC OK");
        }
    }
    action Frame_Start {
        LOGDX("wireformat: Frame_Start");
        start = fpc;
    }
    action Send {
        LOGDX("wireformat: Lending frame to 0");
        sus_lend(0, buf, bytes);
    }

    frame = stx @LRC_Init ((any-etx) @LRC_Byte >Frame_Start) ((any-etx) @LRC_Byte )* (etx @LRC_Byte) any @LRC_Check @Send;
    main := ((any-stx)* frame any)*;
}%%

%% write data;

int fsm_wireformat(void*) {
    char* start = NULL, * end = NULL;
    (void)end, (void)start, (void)wireformat_en_main, (void)wireformat_error, (void)wireformat_first_final;
    char lrc;
    ssize_t bytes;
    char buf[1024] = {};
    volatile int cs;
    char* p = buf, * pe = p;
    %% write init;
    while ((bytes = sus_read(STDIN_FILENO, buf, sizeof buf)) > 0) {
        LOGDX("Received bytes: %*s", bytes, buf);
        pe = buf + bytes;
        %% write exec;
        LOGDX("Bytes processed");
    }
    close(STDIN_FILENO);
    return 0;
}
