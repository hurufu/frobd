#include "log.h"
#include "utils.h"
#include "frob.h"
#include "npthfix.h"
#include <unistd.h>
#include <signal.h>
#include <sys/resource.h>
#include <pthread.h>

int main(const int ac, const char* av[static const ac]) {
    if (ac != 3)
        return 1;
    int fd_fo_main = STDOUT_FILENO, fd_fi_main = STDIN_FILENO;
    ucsp_info_and_adjust_fds(&fd_fo_main, &fd_fi_main);
    int frontend_pipe[2];
    xpipe(frontend_pipe);
    struct ThreadBag thr[] = {
        npth_define(fsm_wireformat, "wp", .from_wire = fd_fi_main, .to_wire = fd_fo_main, .to_fronted = STDOUT_FILENO)
    };
    for (size_t i = 0; i < lengthof(thr); i++) {
        pthread_create(&thr[i].handle, NULL, thr[i].entry, thr[i].arg);
        pthread_setname_np(thr[i].handle, thr[i].name);
    }
    for (size_t i = 0; i < lengthof(thr); i++)
        pthread_join(thr[i].handle, NULL);
    return EXIT_SUCCESS;
}
