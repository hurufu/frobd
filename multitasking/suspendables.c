#include "sus.h"
#include "coro/coro.h"
#include "contextring.h"
#include "eventloop.h"
#include "../log.h"
#include "../utils.h"
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <errno.h>

enum ioset { IOSET_READ, IOSET_WRITE, IOSET_OOB };

struct coro_stuff {
    struct coro_stack stack;
    struct coro_context ctx;
};

union fdset {
    fd_set a[3];
    struct {
        fd_set r, w, e;
    };
};

struct iowork {
    size_t size;
    void* data;
#ifndef NDEBUG
    bool borrowed; // This field is need exclusively for assertions
#endif
    bool disabled;
};

static struct coro_context s_end;
static struct coro_context_ring* s_current;
static struct iowork s_iow[UINT8_MAX];
static bool s_io_surrended;

static struct fdsets {
    union fdset scheduled, active;
} s_iop;

static const char* ioset_to_method(const enum ioset set) {
    switch (set) {
        case IOSET_READ: return "read";
        case IOSET_WRITE: return "write";
        case IOSET_OOB: return "oob";
    }
    return NULL;
}

static void suspend(void) {
    assert(s_current->visited < 100); // EDEADLK
    s_current->visited++;
    coro_transfer(s_current->ctx, &s_end);
}

static void suspend_only(const char* const method) {
    LOGDX("suspended %s at %s", s_current->name, method);
    suspend();
    LOGDX("awaken %s at %s", s_current->name, method);
}

static void suspend_io(const int fd, const enum ioset set) {
    const char* const method = ioset_to_method(set);
    LOGDX("suspended %s at %s fd %d", s_current->name, method, fd);
    suspend();
    LOGDX("awaken %s at %s fd %d", s_current->name, method, fd);
}

static void suspend_io_errno(const int fd, const enum ioset set) {
    const char* const method = ioset_to_method(set);
    LOGD("suspended %s at %s fd %d", s_current->name, method, fd);
    suspend();
    LOGDX("awaken %s at %s fd %d", s_current->name, method, fd);
}

static void suspend_id(const char* const method, const int id) {
    LOGDX("suspended %s at %s id %d", s_current->name, method, id);
    suspend();
    LOGDX("awaken %s at %s id %d", s_current->name, method, id);
}

static void suspend_until_active(const int fd, const enum ioset set) {
    if (s_io_surrended) {
        LOGEX("I/O imposible");
        return;
    }
    while (!FD_ISSET(fd, &s_iop.active.a[set])) {
        if (s_io_surrended) {
            LOGEX("I/O imposible");
            return;
        }
        FD_SET(fd, &s_iop.scheduled.a[set]);
        suspend_io(fd, set);
    }
}

ssize_t sus_write(const int fd, const void* const data, const size_t size) {
again:
    suspend_until_active(fd, IOSET_WRITE);
    const ssize_t r = write(fd, data, size);
    if (r < 0 && errno == EAGAIN) {
        suspend_io_errno(fd, IOSET_WRITE);
        goto again;
    }
    s_current->visited = 0;
    return r;
}

ssize_t sus_read(const int fd, void* const data, const size_t size) {
again:
    suspend_until_active(fd, IOSET_READ);
    const ssize_t r = read(fd, data, size);
    if (r < 0 && errno == EAGAIN) {
        suspend_io_errno(fd, IOSET_READ);
        goto again;
    }
    s_current->visited = 0;
    FD_CLR(fd, &s_iop.active.r);
    //FD_CLR(fd, &s_iop.scheduled.r);
    return r;
}

void sus_disable(const uint8_t id) {
    s_iow[id].disabled = true;
}

void sus_lend(const uint8_t id, const size_t size, void* const data) {
    assert(data != NULL);
    assert(!s_iow[id].borrowed);
    assert(s_iow[id].data == NULL);
    assert(s_iow[id].size == 0);
    s_iow[id] = (struct iowork){ .data = data, .size = size };
    //LOGDX("s_iow[%d].data = %p, data = %p", id, s_iow[id].data, data);
    while (s_iow[id].data != NULL) {
        //LOGDX("suspend because s_iow[%d].data (%p) != NULL", id, s_iow[id].data);
        suspend_id("lend", id);
    }
    //LOGDX("wakeup [%d] = %p", id, s_iow[id].data);
    assert(s_iow[id].data == NULL);
    assert(s_iow[id].size == 0);
    s_current->visited = 0;
}

ssize_t sus_borrow(const uint8_t id, void** const data) {
    //LOGDX("s_iow[%d].data = %p, data = %p", id, s_iow[id].data, data);
    assert(data != NULL);
    assert(!s_iow[id].borrowed);
    if (s_iow[id].disabled) {
        errno = EIDRM;
        return -1;
    }
    while (s_iow[id].data == NULL) {
        //LOGDX("suspend because s_iow[%d].data (%p) == NULL", id, s_iow[id].data);
        if (s_iow[id].disabled) {
            errno = EIDRM;
            return -1;
        }
        suspend_id("borrow", id);
    }
    //LOGDX("wakeup [%d] = %p", id, s_iow[id].data);
#   ifndef NDEBUG
    s_iow[id].borrowed = true;
#   endif
    *data = s_iow[id].data;
    s_current->visited = 0;
    return s_iow[id].size;
}

void sus_return(const uint8_t id, const void* const data, const size_t size) {
    assert(data != NULL);
    assert(s_iow[id].borrowed);
    assert(s_iow[id].data != NULL);
    assert(s_iow[id].data == data);
    assert(s_iow[id].size == size);
    //LOGDX("[%d] %p –> (nil)", id, s_iow[id].data);
    s_iow[id] = (struct iowork){};
    s_current->visited = 0;
    //suspend("return");
}

// Transfer to scheduler and forget about current coroutine
__attribute__((noreturn))
static inline void sus_exit(void) {
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
    sus_exit();
}

int sus_ioloop(struct sus_ioloop_args* const args) {
    io_wait_f* const iowait = get_io_wait(args->timeout);
    struct io_params iop = {
        .maxfd = 10,
        .deadline = comp_deadline(args->timeout)
    };
    int ret = 0;
    do {
again:
        if (s_current == s_current->next) {
            LOGEX("No more tasks left");
            break;
        }
        s_iop = (struct fdsets){ .active = { .r = iop.set[0], .w = iop.set[1], .e = iop.set[2] }};
        suspend_only("iowait");
        bool ok = false;
        for (int i = 0; i < 3; i++) {
            iop.set[i] = s_iop.scheduled.a[i];
            for (int j = 0; j < iop.maxfd; j++) {
                ok |= FD_ISSET(j, &iop.set[i]);
                //LOGDX("%d:%c:%s", j, (FD_ISSET(j, &iop.set[i]) ? '+' : ' '), ioset_to_method(i));
            }
        }
        if (!ok) {
            //LOGDX("continue");
            goto again;
        }
        assert(ok);
        LOGDX("Waiting for I/O...");
    } while ((ret = iowait(&iop)) > 0);
    if (ret < 0)
        LOGE("iowait failed");
    else if (ret == 0)
        LOGI("iowait done");
    for (int i = 0; i < 10; i++)
        close(i);
    LOGWX("surrended");
    s_io_surrended = true;
    return -1;
}

int sus_runall(const size_t length, struct sus_registation_form (* const h)[length]) {
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
    for (; s_current; s_current = s_current ? s_current->next : NULL)
        coro_transfer(&s_end, s_current->ctx);
    ret = 0;
end:
    for (size_t i = 0; i < length; i++) {
        (void)coro_destroy(&stuff[i].ctx);
        coro_stack_free(&stuff[i].stack);
    }
    (void)coro_destroy(&s_end);
    return ret;
}
