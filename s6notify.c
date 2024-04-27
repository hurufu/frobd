#include "frob.h"
#include "log.h"
#include <unistd.h>

int s6_notify(const struct s6_notify_args* const a) {
    if (a->fd > 0)
        if (write(a->fd, "\n", 1) != 1 || close(a->fd) != 0)
            EXITF("Readiness notification failed");
    return 0;
}
