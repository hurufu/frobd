#include "../log.h"
#include "../utils.h"
#include "../evloop.h"
#include "comultitask.h"
#include "frob.h"
#include <time.h>
#include <unistd.h>

enum channel {
    CHANNEL_FI_MAIN = 0,
    CHANNEL_FO_MAIN = 1
};

struct args_io_loop {
    int s6_notification_fd;
    time_t timeout;
    unsigned routines;
};

void comp_max_fd(struct io_params* const iop) {
    (void)iop;
    //suspend();
}

static enum ioset channel_to_set(const enum channel ch) {
    switch (ch) {
        case CHANNEL_FI_MAIN:
            return IOSET_READ;
        case CHANNEL_FO_MAIN:
            return IOSET_WRITE;
    }
    assert(0);
}

static int map_to_fd(const enum channel ch, enum ioset* const set) {
    static const int map[] = {
        [CHANNEL_FI_MAIN] = STDIN_FILENO,
        [CHANNEL_FO_MAIN] = STDOUT_FILENO
    };
    *set = channel_to_set(ch);
    return map[ch];
}

static int co_io_loop(const struct args_io_loop* const args) {
    struct iowork work[args->routines] = {};

    for (;;) {
        struct io_params iop = {};
        {
            for (size_t i = 0; i < lengthof(work); ) {
                sus_peek(&work[i]);
                enum ioset set;
                const int fd = map_to_fd(work[i].id, &set);
                if (fd < 0) {
                    //sus_return_untouched(work[i].ch);
                    continue;
                }
                FD_SET(fd, &iop.set[set]);
                i++;
            }
        }

        io_wait_f* const waitio = get_io_wait(args->timeout);
        for (;;) {
            comp_max_fd(&iop);
            const int r = waitio(&iop);
            if (r < 0)
                return -1;
            else if (r == 0)
                return 0;
            for (size_t i = 0; i < lengthof(iop.set); i++)
            for (int j = 0; j < iop.maxfd; j++)
                if (FD_ISSET(j, &iop.set[i])) {
                    ;
                    //perform_pending_io();
                }
        }

        {
            //sus_return_all();
        }
    }
    return -1;
}

int main() {
    struct sus_coroutine_reg tasks[] = {
        {
            .stack_size = 0,
            .entry = fsm_wireformat,
            .args = NULL
        },
        {
            .stack_size = 0,
            .entry = (sus_entry)co_io_loop,
            .args = &(struct args_io_loop){
                .timeout = -1,
                .s6_notification_fd = 1,
                .routines = 2
            }
        },
        {
            .stack_size = 0,
            .entry = (sus_entry)fsm_frontend_foreign,
            .args = &(struct args_frontend_foreign){}
        }
    };

    if (sus_runall(lengthof(tasks), &tasks) != 0)
        EXITF("Can't start");
    for (size_t i = 0; i < lengthof(tasks); i++)
        if (tasks[i].result)
            return 100;
    return 0;
}
