#include "comultitask.h"
#include "contextring.h"
#include "../coro/coro.h"
#include "../utils.h"
#include <assert.h>
#include <string.h>
#include <unistd.h>

struct coro_stuff {
    struct coro_stack stack;
    struct coro_context ctx;
};

static struct coro_context s_end;
static struct coro_context_ring* s_current;

static struct shared_data {
    int fd[3];
    struct shared_buf {
        int id;
        ssize_t size;
        void* data;
    } buf;
} s_shared = { .fd = { -1, -1, -1 }, .buf = { .id = -1, .size = 0, .data = NULL } };

static void suspend(void) {
    coro_transfer(s_current->ctx, &s_end);
}

static void suspend_until_fd(const enum fdt first, const enum fdt last, const int fd) {
    for (enum fdt set = first; set <= last; set++)
        while (fd != s_shared.fd[set])
            suspend();
    if (first == last)
        s_shared.fd[first] = -1;
}

static void suspend_until_id(const int id) {
    while (s_shared.buf.id != id)
        suspend();
}

ssize_t sus_write(const int fd, const void* const data, const size_t size) {
    suspend_until_fd(FDT_WRITE, FDT_WRITE, fd);
    return xwrite(fd, data, size);
}

ssize_t sus_read(const int fd, void* const data, const size_t size) {
    suspend_until_fd(FDT_READ, FDT_READ, fd);
    return xread(fd, data, size);
}

int sus_select(const int n, fd_set* restrict r, fd_set* restrict w, fd_set* restrict e, struct timeval* restrict t) {
    suspend_until_fd(FDT_READ, FDT_EXCEPT, -1);
    return xselect(n, r, w, e, t);
}

void sus_lend(const int id, void* const data, const size_t size) {
    suspend_until_id(-1);
    assert(s_shared.buf.id == -1 && s_shared.buf.size < 0 && s_shared.buf.data == NULL);
    s_shared.buf = (struct shared_buf){ .id = id, .data = data, .size = size };
    suspend_until_id(-1);
}

ssize_t sus_borrow(const int id, void** const data) {
    suspend_until_id(id);
    assert(s_shared.buf.id == id && (s_shared.buf.size == 0 || (s_shared.buf.size > 0 && s_shared.buf.data != NULL)));
    *data = s_shared.buf.data;
    return s_shared.buf.size;
}

void sus_return(const int id) {
    assert(s_shared.buf.id == id && s_shared.buf.size > 0 && s_shared.buf.data != NULL);
    s_shared.buf = (struct shared_buf){ .id = -1, .size = -1, .data = NULL };
    suspend();
}

int sus_notify(const enum fdt set, const int fd) {
    s_shared.fd[set] = fd;
    suspend();
    return 0;
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
    return ret;
}
