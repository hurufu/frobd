#include "frob.h"
#include "utils.h"
#include "log.h"
#include "npthfix.h"
#include <unistd.h>

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
        xnpth_write_fancy(a->to_wire, 1, (unsigned char[]){lrc == fc ? ACK : NAK});
        if (lrc == fc)
            xnpth_write_fancy(a->to_fronted, fpc - start, buf);
    }
    action Frame_Start {
        start = fpc;
    }

    frame = stx @LRC_Init ((any-etx) @LRC_Byte >Frame_Start) ((any-etx) @LRC_Byte )* (etx @LRC_Byte) any @LRC_Check;
    main := ((any-stx)* frame)*;
}%%

%% write data;

void* fsm_wireformat(const struct fsm_wireformat_args* const a) {
    int cs;
    ssize_t bytes;
    unsigned char buf[1024], * start, * p, * pe, lrc = 0;
    %% write init;
    while ((pe = (p = buf) + xnpth_read_fancy(a->from_wire, sizeof buf, buf)) != buf) {
        %% write exec;
    }
    for (size_t i = 0; i < lengthof(a->fd); i++)
        xclose(a->fd[i]);
    LOGIX("FSM state: current/entry/error/final %d/%d/%d/%d", cs, wireformat_en_main, wireformat_error, wireformat_first_final);
    return NULL;
}
