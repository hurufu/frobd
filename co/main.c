#include "../log.h"
#include "../utils.h"
#include "comultitask.h"
#include "frob.h"
#include <time.h>
#include <unistd.h>

int main() {
    struct sus_coroutine_reg tasks[] = {
        {
            .name = "wireformat",
            .stack_size = 0,
            .entry = fsm_wireformat,
            .args = NULL
        },
        {
            .name = "frontend",
            .stack_size = 0,
            .entry = (sus_entry)fsm_frontend_foreign,
            .args = &(struct args_frontend_foreign){}
        },
        {
            .name = "io_loop",
            .stack_size = 0,
            .entry = (sus_entry)sus_io_loop,
            .args = &(struct sus_args_io_loop){
                .timeout = 3,
                .s6_notification_fd = 1,
                .routines = 2
            }
        }
    };
    if (sus_runall(lengthof(tasks), &tasks) != 0)
        EXITF("Can't start");
    int ret = 0;
    for (size_t i = 0; i < lengthof(tasks); i++) {
        LOGDX("task %zu returned % 2d (%s)", i, tasks[i].result, tasks[i].name);
        if (tasks[i].result)
            ret = 100;
    }
    return ret;
}
