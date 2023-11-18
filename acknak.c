#include "utils.h"
#include "log.h"
#include <stdio.h>
#include <unistd.h>

struct io_params {
    fd_set e, w, r;
    const unsigned short maxfd;
    const short running_time_sec;
    struct timeval time_left;
};

static int wait_for_io(struct io_params* const iop) {
    struct timeval* const tp = iop->running_time_sec < 0 ? NULL: &iop->time_left;
    FD_SET(STDIN_FILENO, &iop->r);
    const int l = xselect(iop->maxfd, &iop->r, &iop->w, &iop->e, tp);
    if (l == 0 && iop->running_time_sec != 0)
        errno = ETIMEDOUT;
    return l;
}

static void event_loop(struct io_params* const iop) {
    char buf[1024];
    while (wait_for_io(iop)) {
        if (FD_ISSET(STDIN_FILENO, &iop->r)) {
            memset(buf, 0, sizeof buf);
            const int r = xread(STDIN_FILENO, buf, sizeof buf);
            if (!r)
                return;
            fprintf(stdout, "%*s", r, buf);
        }
    }
    LOGE("End");
}

int main(const int ac, const char* av[static const ac]) {
    init_log();
    struct io_params iop = {
        .maxfd = 3,
        .running_time_sec = (ac == 2 ? atoi(av[1]) : 3)
    };
    iop.time_left.tv_sec = iop.running_time_sec;
    FD_ZERO(&iop.r);
    FD_ZERO(&iop.w);
    FD_ZERO(&iop.e);
    event_loop(&iop);
    return EXIT_SUCCESS;
}
