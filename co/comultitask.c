#include "comultitask.h"
#include "contextring.h"
#include "../coro/coro.h"
#include "../utils.h"
#include "../evloop.h"
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>

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
};

static struct coro_context s_end;
static struct coro_context_ring* s_current;
static struct iowork s_iow[UINT8_MAX];

static struct fdsets {
    union fdset scheduled, active;
} s_iop;

static void suspend() {
    coro_transfer(s_current->ctx, &s_end);
}

static void suspend_until_active(const int fd, const enum ioset set) {
    while (!FD_ISSET(fd, &s_iop.active.a[set])) {
        FD_SET(fd, &s_iop.scheduled.a[set]);
        suspend();
    }
}

ssize_t sus_write(const int fd, const void* const data, const size_t size) {
    suspend_until_active(fd, IOSET_WRITE);
    return write(fd, data, size);
}

ssize_t sus_read(const int fd, void* const data, const size_t size) {
    suspend_until_active(fd, IOSET_READ);
    return read(fd, data, size);
}

void sus_lend(const uint8_t id, const size_t size, void* const data) {
    assert(data != NULL);
    assert(!s_iow[id].borrowed);
    assert(s_iow[id].data == NULL);
    assert(s_iow[id].size == 0);
    s_iow[id] = (struct iowork){ .data = data, .size = size };
    while (s_iow[id].data != NULL)
        suspend();
    assert(s_iow[id].data == NULL);
    assert(s_iow[id].size == 0);
}

ssize_t sus_borrow(const uint8_t id, void** const data) {
    assert(data != NULL);
    assert(!s_iow[id].borrowed);
    while (s_iow[id].data == NULL)
        suspend();
#   ifndef NDEBUG
    s_iow[id].borrowed = true;
#   endif
    *data = s_iow[id].data;
    return s_iow[id].size;
}

void sus_return(const uint8_t id, const void* const data, const size_t size) {
    assert(data != NULL);
    assert(s_iow[id].borrowed);
    assert(s_iow[id].data != NULL);
    assert(s_iow[id].data == data);
    assert(s_iow[id].size == size);
    s_iow[id] = (struct iowork){};
    suspend();
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
static void starter(struct sus_coroutine_reg* const reg) {
    reg->result = reg->entry(reg->args);
    sus_exit();
}

int sus_io_loop(struct sus_args_io_loop* const args) {
    io_wait_f* const iowait = get_io_wait(args->timeout);
    struct io_params iop = {
        .maxfd = 10,
        .deadline = comp_deadline(args->timeout)
    };
    do {
        s_iop = (struct fdsets){ .active = { .r = iop.set[0], .w = iop.set[1], .e = iop.set[2] }};
        suspend();
        for (int i = 0; i < 3; i++)
            iop.set[i] = s_iop.scheduled.a[i];
    } while (iowait(&iop) > 0);
    return -1;
}

int sus_runall(const size_t length, struct sus_coroutine_reg (* const h)[length]) {
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
        insert(&s_current, &stuff[i].ctx);
    for (; s_current; s_current = s_current->next)
        coro_transfer(&s_end, s_current->ctx);
    ret = 0;
end:
    for (size_t i = 0; i < length; i++) {
        coro_destroy(&stuff[i].ctx);
        coro_stack_free(&stuff[i].stack);
    }
    coro_destroy(&s_end);
    return ret;
}
