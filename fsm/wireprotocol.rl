#include "multitasking/sus.h"
#include "frob.h"
#include "utils.h"
#include "log.h"
#include <unistd.h>
#include <fcntl.h>

%%{
    machine wireformat;
    alphtype unsigned char;
    include frob_common "common.rl";

    action LRC_Init {
        lrc = 0;
    }
    action LRC_Byte {
        lrc ^= fc;
    }
    action LRC_Check {
        if (lrc != fc) {
            LOGDX("@@@ NOK");
            sio_write(args->outfd, "\x03", 1);
        } else {
            LOGDX("@@@ OK");
            sio_write(args->outfd, buf, fpc - start);
        }
    }
    action Frame_Start {
        start = fpc;
    }

    frame = stx @LRC_Init ((any-etx) @LRC_Byte >Frame_Start) ((any-etx) @LRC_Byte )* (etx @LRC_Byte) any @LRC_Check;
    main := ((any-stx)* frame)*;
}%%

%% write data;

int fsm_wireformat(const struct fsm_wireformat_args* const args) {
    unsigned char* start = NULL;
    char lrc;
    ssize_t bytes;
    unsigned char buf[1024];
    int cs;
    unsigned char* p = buf, * pe = p;
    %% write init;
    while ((bytes = sio_read(args->infd, buf, sizeof buf)) > 0) {
        pe = (p = buf) + bytes;
        LOGDXP(char tmp[4*bytes], "→ % 4zd %s", bytes, PRETTY(p, pe, tmp));
        %% write exec;
    }
    if (bytes < 0)
        LOGE("read");
    LOGIX("FSM state: current/entry/error/final %d/%d/%d/%d", cs, wireformat_en_main, wireformat_error, wireformat_first_final);
    return cs == wireformat_error ? -1 : 0;
}
