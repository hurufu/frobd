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
        lrc = 0;
    }
    action LRC_Byte {
        lrc ^= fc;
    }
    action LRC_Check {
        if (lrc != fc) {
            LOGDX("LRC NOK");
            fbreak;
        } else {
            end = fpc;
            LOGDX("LRC OK");
        }
    }
    action Frame_Start {
        start = fpc;
    }
    action Send {
        const int out = 0;
        LOGDX("Lending frame to % 2d % 2zd %p", out, bytes, buf);
        sus_lend(out, bytes, buf);
    }

    frame = stx @LRC_Init ((any-etx) @LRC_Byte >Frame_Start) ((any-etx) @LRC_Byte )* (etx @LRC_Byte) any @LRC_Check @Send;
    main := ((any-stx)* frame)*;
}%%

%% write data;

int fsm_wireformat(void*) {
    unsigned char* start = NULL, * end = NULL;
    (void)start, (void)end;
    char lrc;
    ssize_t bytes = 0;
    unsigned char buf[1024] = {};
    int cs;
    unsigned char* p = buf, * pe = p;
    %% write init;
    while (true) {
        bytes = sus_read(STDIN_FILENO, buf, sizeof buf);
        if (bytes <= 0)
            break;
        LOGDXP(char tmp[4*bytes], "→ % 4zd %s", bytes, PRETTY(buf, buf + bytes, tmp));
        pe = buf + bytes;
        %% write exec;
    }
    if (bytes < 0)
        LOGW("stdin closed");
    LOGIX("FSM state: current/entry/error/final %d/%d/%d/%d", cs, wireformat_en_main, wireformat_error, wireformat_first_final);
    sus_disable(0);
    close(STDIN_FILENO);
    return cs == wireformat_error ? -1 : 0;
}
