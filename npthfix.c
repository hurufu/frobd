#include "npthfix.h"

int npth_sigwait(const sigset_t *set, int *sig) {
    npth_unprotect();
    const int res = sigwait(set, sig);
    npth_protect();
    return res;
}
