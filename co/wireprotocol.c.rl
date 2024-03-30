#include "comultitask.h"
#include "frob.h"
#include "../utils.h"
#include "../log.h"
#include <unistd.h>

%%{
    machine wireformat;
    alphtype unsigned char;
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
            end = fpc;
            LOGDX("wireformat: LRC OK");
        }
    }
    action Log_Byte {
        LOGDX("wireformat: Byte skipped");
    }
    action Frame_Start {
        LOGDX("wireformat: Frame_Start");
        start = fpc;
    }
    action Send {
        const int out = 0;
        LOGDX("wireformat: Lending frame to % 2d % 2zd %p", out, bytes, buf);
        sus_lend(4, bytes, buf);
    }

    frame = stx @LRC_Init ((any-etx) @LRC_Byte >Frame_Start) ((any-etx) @LRC_Byte )* (etx @LRC_Byte) any @LRC_Check @Send;
    main := (((any-stx) @Log_Byte)* frame)*;
}%%

%% write data;

int fsm_wireformat(void*) {
    unsigned char* start = NULL, * end = NULL;
    (void)wireformat_en_main, (void)wireformat_error, (void)wireformat_first_final;
    char lrc;
    ssize_t bytes = 0;
    unsigned char buf[1024] = {};
    volatile int cs;
    unsigned char* p = buf, * pe = p;
    %% write init;
    (void)pe, (void)bytes, (void)lrc, (void)start, (void)end;
    while (true) {
        sus_lend(3, sizeof buf, buf);
        LOGDXP(char tmp[4*bytes], "→ % 4zd %s", bytes, PRETTY(buf, buf + bytes, tmp));
        pe = buf + bytes;
        %% write exec;
        LOGDX("Bytes processed");
    }
    close(STDIN_FILENO);
    LOGWX("STDIN closed. FSM state: current/entry/error/final %d/%d/%d/%d", cs, wireformat_en_main, wireformat_error, wireformat_first_final);
    return cs == wireformat_error ? -1 : 0;
}
