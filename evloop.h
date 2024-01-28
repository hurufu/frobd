#pragma once

#include <sys/select.h>

struct io_params {
    fd_set e, w, r;
    const unsigned short maxfd;
    const struct timeval deadline;
};

typedef int io_wait_f(struct io_params*);

struct timeval comp_deadline(time_t relative);

 __attribute__((__pure__))
io_wait_f* get_io_wait(time_t timeout);
