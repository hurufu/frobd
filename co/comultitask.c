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
static struct iowork s_work; // TODO: Replace with a linked list of iowork

static void suspend() {
    coro_transfer(s_current->ctx, &s_end);
}

void sus_lend(const int id, const size_t size, void* const data) {
    assert(data);
    // Wait for until s_work is free
    while (s_work.id != 0)
        suspend();
    // Publish my data
    s_work = (struct iowork){ .id = id, .size = size, .data = data };
    // Wait until data is borrowed
    while (s_work.id != 0)
        suspend();
    // Wait until data is returned
    while (s_work.id != id)
        suspend();
    s_work = (struct iowork){};
}

void sus_peek(struct iowork* const w) {
    assert(w);
    // Wait until there is something published
    while (s_work.id == 0)
        suspend();
    *w = s_work;
    // Free-up space for a new work
    s_work = (struct iowork){};
}

ssize_t sus_borrow(const int id, void** const data) {
    assert(data);
    // Wait until id is published
    while (s_work.id != id)
        suspend();
    *data = s_work.data;
    const ssize_t size = s_work.size;
    // Free-up space for a new work
    s_work = (struct iowork){};
    return size;
}

void sus_return(const int id) {
    assert(id == s_work.id);
    s_work = (struct iowork){ .id = id };
    // Wait until work was returned
    while (s_work.id != 0)
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
    assert(!s_current);
end:
    for (size_t i = 0; i < length; i++) {
        coro_destroy(&stuff[i].ctx);
        coro_stack_free(&stuff[i].stack);
    }
    coro_destroy(&s_end);
    return ret;
}
