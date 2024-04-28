#include "eventloop.h"
#include "../utils.h"
#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>
#include <stdio.h>

static char* print_ioparams(const struct io_params* const iop, const size_t size, char buf[static const size]) {
    FILE* const s = fmemopen(buf, size, "w");
    assert(s);
    for (int i = 0; i < 3; i++) {
        xfprintf(s, "%c:", (i == 0 ? 'r' : i == 1 ? 'w' : 'e'));
        for (unsigned short fd = 0; fd < iop->maxfd; fd++)
            xfprintf(s, "%d", FD_ISSET(fd, &iop->set[i]));
        xfputs(" ", s);
    }
    xfclose(s);
    return buf;
}

static inline int iselect(struct io_params* const iop, struct timeval* const deadline) {
    LOGDXP(char tmp[64], "%s", print_ioparams(iop, sizeof tmp, tmp));
    return xselect(iop->maxfd, &iop->set[0], &iop->set[1], &iop->set[2], deadline);
}

static int io_wait_indefinitely(struct io_params* const iop) {
    return iselect(iop, NULL);
}

static int io_nowait(struct io_params* const iop) {
    return iselect(iop, &(struct timeval){});
}

static int io_wait_timeout(struct io_params* const iop) {
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

 __attribute__((__pure__))
io_wait_f* get_io_wait(const time_t timeout) {
    return timeout < 0 ? io_wait_indefinitely : (timeout > 0 ? io_wait_timeout : io_nowait);
}

struct timeval comp_deadline(const time_t relative) {
    struct timeval tmp = { .tv_sec = relative, .tv_usec = 0 };
    if (tmp.tv_sec > 0) {
        struct timeval now;
        gettimeofday(&now, NULL);
        timeradd(&now, &tmp, &tmp);
    }
    return tmp;
}
