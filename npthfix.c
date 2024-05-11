#include "npthfix.h"
#include "utils.h"
#include "log.h"

int npth_sigwait(const sigset_t *set, int *sig) {
    npth_unprotect();
    const int res = sigwait(set, sig);
    npth_protect();
    return res;
}

size_t xnpth_write_fancy(const int fd, const size_t size, const unsigned char buf[static const size]) {
    LOGDXP(char tmp[4*size], "[%02d] ← % 4zu %s", fd, size, PRETTY(buf, buf + size, tmp));
    return xnpth_write(fd, buf, size);
}

size_t xnpth_read_fancy(const int fd, const size_t size, unsigned char buf[static const size]) {
    const size_t bytes = xnpth_read(fd, buf, size);
    LOGDXP(char tmp[4*bytes], "[%02d] → % 4zu %s", fd, bytes, PRETTY(buf, buf + bytes, tmp));
    return bytes;
}
