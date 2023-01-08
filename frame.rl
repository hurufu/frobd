#include "frob.h"
#include "log.h"
#include <errno.h>
#include <stddef.h>

%%{
    machine frob_frame;
    include frob_common "common.rl";

    access st->;
    variable p st->p;
    variable pe st->pe;

    action LRC_Byte {
        st->lrc ^= fc;
    }
    action LRC_Check {
        if (st->lrc != fc) {
            LOGWX("Frame LRC failed. Excpected: %#04x != Received: %#04x", st->lrc, fc);
            error = 1; // FIXME: Remove this lame variable
            fbreak;
        }
    }
    action Start {
        start = fpc;
    }
    action End {
        end = fpc;
    }

    frame = stx ((any-etx) @LRC_Byte >Start) ((any-etx) @LRC_Byte )* (etx @LRC_Byte >End) any @LRC_Check;

    main := (any-stx)* frame;

    write data;
}%%

int frob_frame_process(struct frob_frame_fsm_state* const st) {
    // It's better to crash with NULL pointer dereference than have an UB
    unsigned char* start = NULL, * end = NULL;
    if (!st->not_first) {
        %% write init;
        st->lrc = 0;
    }
    st->not_first = true;

    int error = 0;

    %% write exec;

    st->p = start;
    st->pe = end;

    if (error)
        return EBADMSG;
    if (frob_frame_error == st->cs)
        return -1;
    if (frob_frame_first_final == st->cs)
        return 0;
    return EAGAIN;
}
