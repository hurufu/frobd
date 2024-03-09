#include "comultitask.h"
#include "frob.h"
#include "../log.h"
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
    action Send { sus_lend(0, buf, bytes); }

    frame = stx @LRC_Init ((any-etx) @LRC_Byte >Frame_Start) ((any-etx) @LRC_Byte )* (etx @LRC_Byte) any @LRC_Check @Send;
    main := ((any-stx)* frame any)*;
}%%

%% write data;

int fsm_wireformat(const struct coro_args* const ca, void*) {
    char* start = NULL, * end = NULL;
    (void)end, (void)start, (void)wireformat_en_main, (void)wireformat_error, (void)wireformat_first_final;
    char lrc;
    ssize_t bytes;
    char buf[1024];
    int cs;
    char* p = buf, * pe = p;
    %% write init;
    while ((bytes = sus_read(ca->fd[0], buf, sizeof buf)) > 0) {
        %% write exec;
    }
    close(ca->fd[0]);
    return 0;
}
