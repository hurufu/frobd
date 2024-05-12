#include "npthfix.h"
#include "utils.h"
#include "log.h"
#include <pthread.h>
#include <assert.h>

extern size_t xsend_message_buf(int fd, size_t size, const input_t buf[static size]);

size_t xsend_message(const int fd, const input_t* const p, const input_t* const pe) {
    assert(p);
    assert(pe);
    assert(pe >= p);
    const size_t size = pe - p;
    const ssize_t written = write(fd, p, size);
    if (written < 0)
        EXITFP(char tmp[4*size], "[%02d] ↚ % 4zu %s", fd, size, PRETTY(p, pe, tmp));
    LOGDXP(char tmp[4*written], "[%02d] ← % 4zd %*s%s", fd, written, written, PRETTY(p, pe, tmp), (written < size ? " ..." : ""));
    return written;
}

size_t xrecv_message(const int fd, const size_t size, inptut_t p[static const size], const input_t** const pe) {
    assert(p);
    assert(pe);
    assert(*pe);
    const ssize_t bytes = read(fd, p, size);
    if (bytes < 0)
        EXITF("[%02d] ↛", fd);
    *pe = p + bytes;
    LOGDXP(char tmp[4*bytes], "[%02d] → % 4zu %s", fd, bytes, PRETTY(p, *pe, tmp));
    return bytes;
}
