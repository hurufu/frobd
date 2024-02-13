#include "../log.h"
#include "../evloop.h"
#include "../utils.h"
#include "comultitask.h"
#include "frob.h"
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

static struct sus_coroutine_reg coroutines[] = {
    { .stack_size = 4 * 1024, .entry = (sus_entry)co_io_loop, .args = &(struct args_io_loop){
        .s6_notification_fd = 2,
        .timeout = 1
    }},
    { .stack_size = 64, .entry = (sus_entry)fsm_wireformat, .args = &(struct args_wireformat){
        .fdin = STDIN_FILENO,
        .idto = 11
    }},
    { .stack_size = 64, .entry = (sus_entry)fsm_frontend_foreign, &(struct args_frontend_foreign){
        .idfrom = 11,
        .idto = 12
    }},
    { .stack_size = 64, .entry = (sus_entry)fsm_frontend_internal, &(struct args_frontend_internal){
        .idfrom = 12,
        .fdout = STDOUT_FILENO
    }},
    { .stack_size = 64, .entry = (sus_entry)fsm_frontend_timer, &(struct args_frontend_timer){
    }}
};

int main() {
    if (sus_jumpstart(lengthof(coroutines), coroutines) != 0)
        EXITF("Can't start");
    for (int i = 0; i < lengthof(coroutines); i++)
        if (coroutines[i].result)
            return 100;
    return 0;
}
