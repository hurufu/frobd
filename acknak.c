#include "utils.h"
#include "log.h"
#include "evloop.h"
#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>

struct acknak_state {
    int fd;
    input_t* cur;
    input_t buf[256];
};

static void acknak_event_loop(const time_t timeout) {
    struct io_params iop = {
        .maxfd = 3,
        .deadline = comp_deadline(timeout)
    };
    FD_ZERO(&iop.r);
    FD_ZERO(&iop.w);
    FD_ZERO(&iop.e);

    struct acknak_state st;

    FD_SET(STDIN_FILENO, &iop.r);
    char buf[1024], * cur = buf;
    struct frob_frame_fsm_state st;
    frob_frame_process(&st);

    io_wait_f* const io_wait = get_io_wait(timeout);
    while ((*io_wait)(&iop)) {
        if (FD_ISSET(STDIN_FILENO, &iop.r)) {
            const int r = xread(STDIN_FILENO, cur, cur - buf + sizeof buf);
            if (!r)
                return;
            cur += r;
        }
        if (cur < buf + sizeof buf)
            FD_SET(STDIN_FILENO, &iop.r);
        else
            FD_CLR(STDIN_FILENO, &iop.r);

        for (frob_frame_process()) {
            commission_ack_on_main(e);
            if (0 == e) {
                if (parse_message()) {
            }
        }

    }

    LOGE("End");
}

int main(const int ac, const char* av[static const ac]) {
    acknak_event_loop(ac == 2 ? atoi(av[1]) : -1);
    return 0;
}
