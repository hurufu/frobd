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

void sus_lend(const int id, const size_t size, void* const data) {
    assert(id);
    assert(data);
    (void)size;
}

ssize_t sus_borrow(enum channel* const id, void** const data) {
    assert(id);
    assert(data);
    return -1;
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
