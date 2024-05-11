#include "log.h"
#include "utils.h"
#include "frob.h"
#include "npthfix.h"
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
    if (ac != 3)
        return 1;
    adjust_rlimit();
    int fd_fo_main = STDOUT_FILENO, fd_fi_main = STDIN_FILENO;
    ucsp_info_and_adjust_fds(&fd_fo_main, &fd_fi_main);
    int frontend_pipe[2];
    xpipe(frontend_pipe);
    struct ThreadBag thr[] = {
        npth_define(fsm_wireformat, .in = fd_fi_main, .out = frontend_pipe[1]),
        npth_define(fsm_frontend_foreign, .in = frontend_pipe[0])
    };
    xnpth_init();
    npth_sigev_init();
    npth_sigev_add(SIGPWR);
    npth_sigev_fini();
    for (size_t i = 0; i < lengthof(thr); i++) {
        xnpth_create(&thr[i].handle, NULL, thr[i].entry, thr[i].arg);
        xnpth_setname_np(thr[i].handle, thr[i].name);
    }
    for (;;) {
        int sig;
        xnpth_sigwait(npth_sigev_sigmask(), &sig);
        switch (sig) {
            case SIGPWR:
                LOGD("got %s", strsignal(sig));
                break;
            default:
                break;
        }
    }
    return 0;
}
