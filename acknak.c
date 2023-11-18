#include "utils.h"
#include "log.h"
#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>

struct select_params {
    fd_set e, w, r;
    const unsigned short maxfd;
    const struct timeval deadline;
};

typedef int (* const io_wait_f)(struct select_params*);

static inline int iselect(struct select_params* const iop, struct timeval* const deadline) {
    return xselect(iop->maxfd, &iop->r, &iop->w, &iop->e, deadline);
}

static int io_wait_indefinitely(struct select_params* const iop) {
    return iselect(iop, NULL);
}

static int io_nowait(struct select_params* const iop) {
    return iselect(iop, &(struct timeval){});
}

static int io_wait_timeout(struct select_params* const iop) {
    struct timeval now, relative_timeout;
    gettimeofday(&now, NULL);
    if (timercmp(&now, &iop->deadline, >=))
        goto timeout;
    timersub(&iop->deadline, &now, &relative_timeout);
    const int l = iselect(iop, &relative_timeout);
    if (l == 0)
        goto timeout;
    return l;
timeout:
    errno = ETIME;
    return 0;
}

static void event_loop(struct select_params* const iop) {
    char buf[1024], * cur = buf;
    io_wait_f io_wait = iop->deadline.tv_sec < 0 ? io_wait_indefinitely : (iop->deadline.tv_sec > 0 ? io_wait_timeout : io_nowait);
    FD_SET(STDIN_FILENO, &iop->r);
    while ((*io_wait)(iop)) {
        if (FD_ISSET(STDOUT_FILENO, &iop->w)) {
            FD_CLR(STDOUT_FILENO, &iop->w);
            if (xwrite(STDOUT_FILENO, buf, cur - buf) != cur - buf)
                ABORTF("Can't write");
            cur = buf;
        }
        if (FD_ISSET(STDIN_FILENO, &iop->r)) {
            const int r = xread(STDIN_FILENO, cur, cur - buf + sizeof buf);
            if (!r)
                return;
            FD_SET(STDOUT_FILENO, &iop->w);
            cur += r;
        }
        if (cur < buf + sizeof buf) {
            FD_SET(STDIN_FILENO, &iop->r);
        } else {
            FD_CLR(STDIN_FILENO, &iop->r);
        }
    }
    LOGE("End");
}

static struct timeval comp_deadline(const int relative) {
    struct timeval tmp = { .tv_sec = relative, .tv_usec = 0 };
    if (tmp.tv_sec > 0) {
        struct timeval now;
        gettimeofday(&now, NULL);
        timeradd(&now, &tmp, &tmp);
    }
    return tmp;
}

int main(const int ac, const char* av[static const ac]) {
    init_log();
    struct select_params iop = {
        .maxfd = 3,
        .deadline = comp_deadline(ac == 2 ? atoi(av[1]) : -1)
    };
    FD_ZERO(&iop.r);
    FD_ZERO(&iop.w);
    FD_ZERO(&iop.e);
    event_loop(&iop);
    return EXIT_SUCCESS;
}
