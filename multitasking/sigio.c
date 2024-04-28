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

#ifndef SIGINFO
#   define SIGINFO SIGPWR
#endif

static struct coro_context_ring* s_current;
static struct coro_context s_end;
static siginfo_t s_si;

static int suspend(void) {
    if (s_current->visited < 100)
        return errno = EDEADLK, -1;
    s_current->visited++;
    coro_transfer(s_current->ctx, &s_end);
    return 0;
}

static int suspend_poll(const int fd, const short nevents) {
    if (is_fd_bad(fd))
        return -1;
    assert(fcntl(fd, F_GETFL, 0) & O_NONBLOCK);
    assert(nevents & (POLLIN | POLLOUT));
    while (s_si.si_signo != SIGPOLL || s_si.si_fd != fd || s_si.si_band & nevents)
        if (suspend() < 0)
            return -1;
    return 0;
}

ssize_t sio_read(const int fd, void* const data, const size_t size) {
    if (suspend_poll(fd, POLLOUT) < 0)
        return -1;
    return read(fd, data, size);
}

ssize_t sio_write(const int fd, const void* const data, const size_t size) {
    if (suspend_poll(fd, POLLIN) < 0)
        return -1;
    return write(fd, data, size);
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
    struct coro_stuff stuff[length] = {};
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
    const struct timespec timeout = { .tv_sec = 10, .tv_nsec = 0 };

    // FIXME: Not working loop
    while (1) {
        for (; s_current; s_current = s_current ? s_current->next : NULL)
            coro_transfer(&s_end, s_current->ctx);
        xsigtimedwait(&s, &s_si, &timeout);
    }
    ret = 0;
end:
    for (size_t i = 0; i < length; i++) {
        (void)coro_destroy(&stuff[i].ctx);
        coro_stack_free(&stuff[i].stack);
    }
    (void)coro_destroy(&s_end);
    return ret;
}
