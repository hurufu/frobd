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
        xsend_message(a->to_fronted, (lrc == fc ? start : fpc), fpc);
    }
    action Frame_Start {
        start = fpc;
    }

    body = any-etx;
    frame = stx @LRC_Init (body @LRC_Byte >Frame_Start) (body @LRC_Byte)* (etx @LRC_Byte) any @LRC_Check;
    main := ((any-stx)* frame)*;
}%%

%% write data;

void* fsm_wireformat(const struct fsm_wireformat_args* const a) {
    input_t buf[1024], * start, * p, lrc = 0;
    const input_t* pe;
    int cs;
    %% write init;
    while (xrecv_message(a->from_wire, sizeof buf, p = buf, &pe) > 0) {
        %% write exec;
    }
    for (size_t i = 0; i < lengthof(a->fd); i++)
        xclose(a->fd[i]);
    LOGIX("FSM state: current/entry/error/final %d/%d/%d/%d", cs, wireformat_en_main, wireformat_error, wireformat_first_final);
    return NULL;
}
