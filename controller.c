#include "frob.h"
#include "log.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>

int controller(struct controller_args*) {
    LOGWX("Master channel doesn't work: %s", strerror(ENOSYS));
    return 0;
}
