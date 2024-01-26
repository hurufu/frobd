#include "utils.h"
#include "log.h"
#include "evloop.h"
#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>

static inline int iselect(struct io_params* const iop, struct timeval* const deadline) {
    return xselect(iop->maxfd, &iop->r, &iop->w, &iop->e, deadline);
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
