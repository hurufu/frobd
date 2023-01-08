#include <errno.h>
#include "frob.h"

%%{
    machine frob_frame;
    alphtype unsigned char;

    access st->;
    variable p st->p;
    variable pe st->pe;

    action LRC_Byte {
        st->lrc ^= fc;
    }
    action LRC_Check {
        if (st->lrc != fc) {
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

    stx = 0x02;
    etx = 0x03;
    frame = stx ((any-etx) @LRC_Byte >Start) ((any-etx) @LRC_Byte )* (etx @LRC_Byte >End) any @LRC_Check;

    main := (any-stx)* frame;

    write data;
}%%

int frob_frame_process(struct frob_frame_fsm_state* const st) {
    const unsigned char* start, * end;
    if (!st->not_first) {
        %% write init;
    }
    st->not_first = true;

    int error = 0;

    %% write exec;

    st->p = start;
    st->pe = end;

    if (error)
        return EBADMSG;
    if (frob_frame_error == st->cs)
        return EBADMSG;
    if (frob_frame_first_final == st->cs)
        return 0;
    return EAGAIN;
}
