#include "../log.h"
#include "../utils.h"
#include "comultitask.h"
#include "frob.h"
#include <time.h>
#include <unistd.h>

int main() {
    struct sus_coroutine_reg tasks[] = {
        {
            .stack_size = 0,
            .entry = fsm_wireformat,
            .args = NULL
        },
        {
            .stack_size = 0,
            .entry = (sus_entry)sus_io_loop,
            .args = &(struct sus_args_io_loop){
                .timeout = -1,
                .s6_notification_fd = 1,
                .routines = 2
            }
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
