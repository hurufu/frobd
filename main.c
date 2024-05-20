#include "log.h"
#include "utils.h"
#include "frob.h"
#include "tasks.h"
#include <unistd.h>
#include <signal.h>
#include <sys/resource.h>
#include <pthread.h>

union iopair {
    int fd[2];
    struct {
        int r, w;
    };
};

static union iopair get_main(void) {
    union iopair ret = { .r = STDIN_FILENO, .w = STDOUT_FILENO };
    ucsp_info_and_adjust_fds(&ret.w, &ret.r);
    return ret;
}

static union iopair make_pipe(void) {
    union iopair ret;
    xpipe(ret.fd);
    return ret;
}

int main(const int ac, const char* av[static const ac]) {
    if (ac != 3)
        return 1;
    const union iopair foreign = get_main(), internal[] = { make_pipe() };
    struct task* t[] = {
        create_task("wp", fsm_wireformat, .from_wire = foreign.r, .to_wire = foreign.w, .to_fronted = internal[0].w)
    };
    int error = 0;
    for (size_t i = 0; i < lengthof(t); i++)
        error |= teardown_task(t[i]);
    return error ? EXIT_FAILURE : EXIT_SUCCESS;
}
