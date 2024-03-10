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

int co_io_loop(const struct args_io_loop* const args) {
    struct io_params iop = { .maxfd = STDIN_FILENO + 1 };
    FD_SET(STDIN_FILENO, &iop.set[0]);
    LOGDX("start with fd[0] %d", STDIN_FILENO);
    io_wait_f* const waitio = get_io_wait(args->timeout);
    while (waitio(&iop)) {
        LOGDX("data received");
        for (size_t i = 0; i < lengthof(iop.set); i++)
        for (int j = 0; j < iop.maxfd; j++)
            if (FD_ISSET(j, &iop.set[i]))
                if (sus_notify(i, j) != 0) {
                    LOGWX("Will close %lu %d", i, j);
                    FD_CLR(j, &iop.set[i]);
                }
    }
    return 0;
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
            .args = &(struct args_io_loop){ .timeout = -1, .s6_notification_fd = 1 }
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
