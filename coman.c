#include "coro/coro.h"
#include "utils.h"

struct coro {
    coro_func entry;
    coro_context ctx;
    struct coro_stack stack;
};

struct io_state {
    size_t size, offset;
    input_t* in, *out, *except;
};

void coroutine_evloop(struct coro* const c) {
    io_wait_f* const io_wait = get_io_wait(1);
    while ((*io_wait)(&iop)) {
        for (int i = 0; i < number_of_channels; i++)
            perform_pending_io();
        for (int i = 0; i < number_of_channels; i++)
            process_channel();
    }
}

#define yield(C) coro_transfer(&(C)[2].ctx, &(C)[1].ctx);

void coroutine_acknak(struct coro* const c) {
    static input_t buf[1024];
    static struct io_state ios = {
        .size = sizeof buf,
        .buf = buf
    };
    ce_register_channel(&ios, CE_READ | CE_WRITE | CE_EXCEPT);

    int lrc = 0;
    %% write init;

    for (;;) {
        yield(c);
        %% write exec;
        if (frame) {
            if (cs == frob_frame_first_final) {
                ios.out = lrc == *pc ? ACK : NAK;
                ios.out_size++;
                if (lrc != *pc)
                    break;
                const size_t frame_size = end - start;
                memcpy(pipe_out_cur, start, frame_size);
                pipe_out_cur += frame_size;
            }
        } if (ack) {
            disarm_timer();
        } if (nak) {
            retransmit();
        }
    }
}

void coroutine_acknak_pipe(struct coro* const c) {
    static input_t buf[1024];
    ce_register_channel(&ios, CE_READ | CE_WRITE | CE_EXCEPT);
    for (;;) {
        yield(c);
        %% write exec
        if (cs == frob_frame_first_final) {
            const size_t frame_size = end - start;
            memcpy(main_out, start, frame_size);
            main_out += frame_size;
            set_ack_timer();
        }
    }
}

void coroutine_acknak_timers(struct coro* const c) {
    static input_t buf[1024];
    int count = 1;
    for (;;) {
        yield(c);
        if (count % 3)
            disarm_timer();
        else
            retransmit();
    }
}

int main() {
    static struct coro c[3] = { {}, { coroutine_evloop }, { coroutine_acknak } };
    coro_create(&ctx[0].ctx, NULL, NULL, NULL, 0);
    for (int i = 1; i < lengthof(c); i++) {
        coro_stack_alloc(&c[i].stack, 0);
        coro_create(&c[i].ctx, c[i].entry, c, c[i].stack.sptr, c[i].stack.ssze);
    }
    coro_transfer(&c[0].ctx, &c[1].ctx);
    for (int i = 1; i < lengthof(c); i++)
        coro_stack_free(&c[i].stack);
    return 0;
}
