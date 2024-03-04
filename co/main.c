#include "../log.h"
#include "../utils.h"
#include "../evloop.h"
#include "comultitask.h"
#include "frob.h"
#include <time.h>
#include <unistd.h>

int g_log_level = LOG_DEBUG;

struct args_io_loop {
    int s6_notification_fd;
    time_t timeout;
};

int co_io_loop(const struct coro_args* const ca, const struct args_io_loop* const args) {
    struct io_params iop = { .maxfd = ca->fd[0] + 1 };
    FD_SET(ca->fd[0], &iop.set[0]);
    LOGDX("start with fd[0] %d", ca->fd[0]);
    io_wait_f* const waitio = get_io_wait(args->timeout);
    while (waitio(&iop)) {
        LOGDX("data received");
        for (size_t i = 0; i < lengthof(iop.set); i++)
        for (int j = 0; j < iop.maxfd; j++)
            if (FD_ISSET(j, &iop.set[i]))
                sus_resume(i, j);
        // Remove closed or if need add a new file descriptor
    }
    return 0;
}

int main() {
    struct sus_coroutine_reg coroutines[] = {
        {
            .stack_size = 0,
            .ca = {
                .fd = (int[]){ STDIN_FILENO },
                .idx = NULL
            },
            .entry = fsm_wireformat,
            .args = NULL
        },
        {
            .stack_size = 0,
            .ca = {
                .fd = (int[]){ STDIN_FILENO },
                .idx = NULL
            },
            .entry = (sus_entry)co_io_loop,
            .args = &(struct args_io_loop){ .timeout = -1, .s6_notification_fd = -1 }
        }
    };

    if (sus_runall(lengthof(coroutines), &coroutines) != 0)
        EXITF("Can't start");
    for (size_t i = 0; i < lengthof(coroutines); i++)
        if (coroutines[i].result)
            return 100;
    return 0;
}
