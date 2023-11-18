#include "utils.h"
#include "log.h"

struct io_params {
    fd_set e, w, r;
    const unsigned short maxfd;
    const short running_time_sec;
};

static int wait_for_io(struct io_params* const iop) {
    static struct timeval t;
    const bool unlimited = iop->running_time_sec < 0;
    if (unlimited && t.tv_sec && t.tv_usec)
        t.tv_sec = iop->running_time_sec;
    const int l = xselect(iop->maxfd, &iop->r, &iop->w, &iop->e, (unlimited ? NULL: &t));
    if (l == 0 && iop->running_time_sec != 0)
        ERRNO(ETIMEDOUT);
    return l;
}

static void event_loop(struct io_params* const iop) {
    while (wait_for_io(iop)) {
        perform_pending_io();
        process_channels();
    }
    LOGD("End");
}

int main(const int ac, const char* av[static const ac]) {
    init_log();
    struct io_params iop = {
        .maxfd = 3,
        .running_time_sec = -1
    };
    event_loop(&iop);
    return EXIT_SUCCESS;
}
