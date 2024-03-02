#include "comultitask.h"
#include "frob.h"
#include "../log.h"
#include <unistd.h>
#include <stdio.h>

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
    action Send { printf("%*s", (int)bytes, buf); }

    frame = stx @LRC_Init ((any-etx) @LRC_Byte >Frame_Start) ((any-etx) @LRC_Byte )* (etx @LRC_Byte) any @LRC_Check @Send;
    main := ((any-stx)* frame any)*;
}%%

%% write data;

int fsm_wireformat(const struct coro_args* const ca, void*) {
    char* start = NULL, * end = NULL;
    unsigned lrc;
    ssize_t bytes;
    char buf[1024];
    int cs;
    int to = 4;
    char* p = buf, * pe = p;
    %% write init;
    while ((bytes = sus_read(ca->fd[0], buf, sizeof buf)) > 0) {
        LOGDX("%*s", (int)bytes, buf);
        %% write exec;
    }
    LOGDX("close");
    close(ca->fd[0]);
}
