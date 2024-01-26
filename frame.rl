#include "frob.h"
#include "log.h"
#include "utils.h"
#include <errno.h>

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

    main := (any-stx)* frame any*;

    write data;
}%%

void frob_frame_init(struct frob_frame_fsm_state* const st) {
    %% write init;
    st->lrc = 0;
}

int frob_frame_process(struct frob_frame_fsm_state* const st) {
    // It's better to crash with NULL pointer dereference than have an UB
    unsigned char* start = st->p, * end = NULL;
    if (!st->not_first) {
        %% write init;
        st->lrc = 0;
    }
    st->not_first = true;

    int error = 0;

    %% write exec;

    st->p = start;
    st->pe = end;

    if (!st->pe)
        return EAGAIN;
    if (error)
        return EBADMSG;
    if (frob_frame_error == st->cs)
        return ENOTRECOVERABLE;
    if (frob_frame_first_final == st->cs)
        return 0;
    return EAGAIN;
}
