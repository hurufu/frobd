#include "log.h"
#include "utils.h"
#include "comultitask.h"
#include "frob.h"

int main() {
    struct sus_registation_form tasks[] = {
        sus_registration(fsm_wireformat),
        sus_registration(fsm_frontend_foreign),
        sus_registration(sus_ioloop, .timeout = 3)
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
