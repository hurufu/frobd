#include "../log.h"
#include "../evloop.h"
#include "../utils.h"
#include "comultitask.h"
#include <time.h>
#include <unistd.h>

struct args_io_loop {
    int s6_notification_fd;
    time_t timeout;
};

int g_log_level = LOG_DEBUG;

static void s6_ready(const int fd) {
    if (write(fd, "\n", 1) != 1 || close(fd) != 0)
        EXITF("Readiness notification failed (%d)", fd);
}

static int co_io_loop(const struct args_io_loop* const args) {
    struct io_params iop = { .maxfd = 3 };
    FD_SET(STDIN_FILENO, &iop.set[0]);
    io_wait_f* const waitio = get_io_wait(args->timeout);
    sus_yield();
    if (args->s6_notification_fd > 0)
        s6_ready(args->s6_notification_fd);
    while (waitio(&iop)) {
        for (int i = 0; i < lengthof(iop.set); i++)
        for (int j = 0; j < iop.maxfd; j++)
            if (FD_ISSET(j, &iop.set[i]))
                sus_resume(i, j);
        // Remove closed or if need add a new file descriptor
    }
    return -1;
}

int main() {
    struct sus_coroutine_reg h[] = {
        { .stack_size = 4 * 1024, .entry = (int (*)(void*))co_io_loop, .args = &(struct args_io_loop){
            .s6_notification_fd = 2,
            .timeout = 1
        }}
    };
    if (sus_jumpstart(lengthof(h), h) != 0)
        EXITF("Can't start");
    for (int i = 0; i < lengthof(h); i++)
        if (h[i].result)
            return 100;
    return 0;
}
