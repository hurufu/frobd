#include "coro/coro.h"
#include "utils.h"
#include "log.h"
#include "contextring.h"
#include "sus.h"
#include <signal.h>
#include <poll.h>
#include <errno.h>
#include <assert.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>

#ifndef SIGINFO
#   define SIGINFO SIGPWR
#endif

static struct coro_context_ring* s_current;
static struct coro_context_ring* s_waiting;
static struct coro_context s_end;
static siginfo_t s_si;

static char* events_tostring(const size_t size, char buf[static const size], const short e) {
    assert(!(e & ~0x37FF));
    FILE* const s = fmemopen(buf, size, "w");
    xfprintf(s, "0x%04hx", e);
#   if 0
    if (e & POLLRDHUP)
        xfputs(" RDHUP", s);
    if (e & POLLREMOVE)
        xfputs(" REMOVE", s);
    if (e & POLLMSG)
        xfputs(" MSG", s);
#   endif
#   ifdef _XOPEN_SOURCE
    if (e & POLLWRBAND)
        xfputs(" XOUTPRI", s);
    if (e & POLLWRNORM)
        xfputs(" XOUT", s);
    if (e & POLLRDBAND)
        xfputs(" XINPRI", s);
    if (e & POLLRDNORM)
        xfputs(" XIN", s);
#   endif
    if (e & POLLNVAL)
        xfputs(" NVAL", s);
    if (e & POLLHUP)
        xfputs(" HUP", s);
    if (e & POLLERR)
        xfputs(" ERR", s);
    if (e & POLLOUT)
        xfputs(" OUT", s);
    if (e & POLLPRI)
        xfputs(" PRI", s);
    if (e & POLLIN)
        xfputs(" IN", s);
    xfclose(s);
    return buf;
}

static int suspend_poll(const int fd, const short wants) {
    LOGDP(char tmp[32], "fd:%d fcntl:%#x wants:%s", fd, fcntl(fd, F_GETFL), events_tostring(sizeof tmp, tmp, wants));
    if (is_fd_bad(fd))
        return -1;
    assert(fcntl(fd, F_GETFL) & O_NONBLOCK);
#   if 0
    assert(fcntl(fd, F_GETOWN) == getpid());
    assert(fcntl(fd, F_GETSIG) == SIGPOLL);
    assert(fcntl(fd, F_GETFL) & O_ASYNC);
#   endif
    while (s_si.si_signo != SIGPOLL || /* s_si.si_fd != fd || */ !(s_si.si_band & wants)) {
        if (s_current->visited > 100)
            return errno = EDEADLK, -1;
        const char* const name = s_current->name;
        insert(&s_waiting, shrink(&s_current), name);
        coro_transfer(s_waiting->ctx, &s_end);
    }
    s_si = (siginfo_t){};
    return 0;
}

ssize_t sio_read(const int fd, void* const data, const size_t size) {
    LOGDX("> %s fd:%d", s_current->name, fd);
    ssize_t res;
    while ((res = read(fd, data, size)) < 0) {
        switch (errno) {
            case EINTR:
                continue;
            case EAGAIN:
                if (suspend_poll(fd, POLLIN | POLLRDNORM) < 0) {
                    return -1;
                }
                break;
            default:
                return -1;
        }
    }
    LOGDX("< %s res:%zd", s_current->name, res);
    return res;
}

ssize_t sio_write(const int fd, const void* const data, const size_t size) {
    LOGDX("> %s fd:%d", s_current->name, fd);
    ssize_t res;
    while ((res = write(fd, data, size)) < 0) {
        switch (errno) {
            case EINTR:
                continue;
            case EAGAIN:
                if (suspend_poll(fd, POLLOUT | POLLWRNORM) < 0) {
                    return -1;
                }
                break;
            default:
                return -1;
        }
    }
    LOGDX("< %s res:%zd", s_current->name, res);
    return res;
}

ssize_t sio_memwrite(int id, const void* const data, const size_t size) {
    const void* const buf = xmalloc(size);
    return -1;
}

// Transfer to scheduler and forget about current coroutine
__attribute__((noreturn))
static inline void sig_exit(void) {
    coro_context* const origin = s_current->ctx;
    shrink(&s_current);
    coro_transfer(origin, &s_end);
    assert(0);
}

__attribute__((noreturn))
static void starter(struct sus_registation_form* const reg) {
    LOGDX("started task %s", reg->name);
    reg->result = reg->entry(reg->args);
    LOGDX("ended task %s", reg->name);
    sig_exit();
}

int sig_runall(const size_t length, struct sus_registation_form (* const h)[length]) {
    assert(h);
    int ret = -1;
    struct coro_stuff stuff[length];
    memset(stuff, 0, length);
    coro_create(&s_end, NULL, NULL, NULL, 0);
    for (size_t i = 0; i < length; i++) {
        if (!coro_stack_alloc(&stuff[i].stack, (*h)[i].stack_size))
            goto end;
        coro_create(&stuff[i].ctx, (coro_func)starter, &(*h)[i], stuff[i].stack.sptr, stuff[i].stack.ssze);
    }
    for (size_t i = 0; i < length; i++)
        insert(&s_current, &stuff[i].ctx, (*h)[i].name);

    static const int blocked_signals[] = { SIGPOLL, SIGURG, SIGINFO, SIGINT};
    sigset_t s;
    sigemptyset(&s);
    for (size_t i = 0; i < lengthof(blocked_signals); i++)
        sigaddset(&s, blocked_signals[i]);
    xsigprocmask(SIG_BLOCK, &s, NULL);
    const struct timespec timeout = { .tv_sec = 100, .tv_nsec = 0 };
    do {
        for (; s_current; s_current = s_current ? s_current->next : NULL)
            coro_transfer(&s_end, s_current->ctx);
        LOGDX("waiting for I/O...");
        const int sig = xsigtimedwait(&s, &s_si, &timeout);
        if (s_si.si_signo == SIGPOLL)
            LOGDXP(char tmp[32], "signal %02d: %s (v: %d band: %s)",
                    sig, strsignal(sig), s_si.si_value.sival_int, events_tostring(sizeof tmp, tmp, s_si.si_band));
        else
            psiginfo(&s_si, NULL);
        s_current = s_waiting;
        s_waiting = NULL;
    } while (s_current);
    ret = 0;
end:
    for (size_t i = 0; i < length; i++) {
        (void)coro_destroy(&stuff[i].ctx);
        coro_stack_free(&stuff[i].stack);
    }
    (void)coro_destroy(&s_end);
    return ret;
}
