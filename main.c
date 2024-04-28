#include "log.h"
#include "utils.h"
#include "multitasking/sus.h"
#include "frob.h"
#include <unistd.h>
#include <signal.h>
#include <sys/resource.h>

static void adjust_rlimit(void) {
    // This will force syscalls that allocate file descriptors to fail if it
    // doesn't fit into fd_set, so we don't have to check for that in the code.
    struct rlimit rl;
    if (getrlimit(RLIMIT_NOFILE, &rl) != 0)
        ABORTF("getrlimit");
    if (rl.rlim_cur > FD_SETSIZE)
        if (setrlimit(RLIMIT_NOFILE, &(struct rlimit){ .rlim_cur = FD_SETSIZE, .rlim_max = rl.rlim_max }) != 0)
            ABORTF("setrlimit");
}

int main(const int ac, const char* av[static const ac]) {
    init_log();
    if (ac != 3)
        return 1;
    adjust_rlimit();
    int fd_fo_main = STDOUT_FILENO, fd_fi_main = STDIN_FILENO;
    ucsp_info_and_adjust_fds(&fd_fo_main, &fd_fi_main);
    struct sus_registation_form tasks[] = {
        sus_registration(fsm_wireformat, fd_fi_main),
        sus_registration(fsm_frontend_foreign),
        sus_registration(fsm_frontend_timer),
        sus_registration(autoresponder, av[2], 1, fd_fo_main),
        sus_registration(sighandler),
        sus_registration(s6_notify, -1),
        sus_registration(controller),
    };
    if (sig_runall(lengthof(tasks), &tasks) != 0)
        EXITF("Can't start");
    int ret = 0;
    for (size_t i = 0; i < lengthof(tasks); i++) {
        LOGDX("task %zu returned % 2d (%s)", i, tasks[i].result, tasks[i].name);
        if (tasks[i].result)
            ret = 100;
    }
    return ret;
}
