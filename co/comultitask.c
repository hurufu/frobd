#include "comultitask.h"
#include "../coro/coro.h"
#include "../log.h"
#include "../utils.h"
#include <assert.h>
#include <string.h>
#include <unistd.h>

static coro_context* s_ctx;
static coro_context* s_current;
static coro_context* s_scheduler;
static coro_context* s_end;

static int fd_to_index(const int set, const int fd) {
    switch (fd) {
        case STDIN_FILENO:
            return 1;
        case STDOUT_FILENO:
            return 2;
    }
    return 0;
}

// Register for writting
ssize_t sus_write(const int fd, const void* const data, const size_t size) {
    //transfer_to_select();
    return write(fd, data, size);
}

// Register for reading
ssize_t sus_read(const int fd, void* const data, const size_t size) {
    return read(fd, data, size);
}

// Wait untile connected coroutine yields
ssize_t sus_lend(const int id, void* const data, const size_t size) {
    return size;
}

// Transfer directly to connected coroutine
ssize_t sus_borrow(const int id, void** const data) {
    return -1;
}

// Transfer to a coroutine that is waiting on a file descriptor
int sus_resume(int set, int fd) {
    coro_context* const origin = s_current, * const destination = s_ctx + fd_to_index(set, fd);
    coro_transfer(origin, destination);
}

// Transfer to scheduler and place current coroutine to a pending list
void sus_yield(void) {
    coro_context* const origin = s_current, * const next = NULL /* ... */;
    s_current = next;
    coro_transfer(origin, next);
}

int sus_wait(void) {
    return 0;
}

// Transfer to scheduler and forget about current coroutine
__attribute__((noreturn))
static inline void sus_return(void) {
    //coro_transfer(origin, s_scheduler);
    assert(0);
}

__attribute__((noreturn))
void co_schedule(void*) {
    size_t number_of_coroutines = 3;
    for (size_t i = 0; sus_wait(); i = (i + 1) % number_of_coroutines) {
        coro_context* const origin = s_current;
        s_current = s_ctx + i;
        coro_transfer(origin, s_current);
    }
    LOGDX("All coroutines returned");
    //coro_transfer(origin, s_end);
    assert(0);
}

__attribute__((noreturn))
static void starter(struct sus_coroutine_reg* const reg) {
    reg->result = reg->entry(reg->args);
    sus_return();
}

int sus_jumpstart(const size_t length, struct sus_coroutine_reg h[static const length]) {
    //assert(first < length);
    assert(h);
    int ret = -1;
    struct {
        struct coro_stack stack;
        coro_context ctx;
    } stuff[length + 2];
    memset(stuff, 0, sizeof stuff);
    if (!coro_stack_alloc(&stuff[1].stack, 0))
        goto end;
    coro_create(&stuff[0].ctx, NULL, NULL, NULL, 0);
    coro_create(&stuff[1].ctx, co_schedule, NULL, stuff[1].stack.sptr, stuff[1].stack.ssze);
    s_end = &stuff[0].ctx;
    s_scheduler = &stuff[1].ctx;
    for (int i = 0; i < length; i++) {
        /*
        if (!coro_stack_alloc(&stuff[i + 2].stack, h[i].stack.ssze))
            goto end;
        coro_create(&s_ctx[i], starter, &h[i], h[i].stack.sptr, h[i].stack_size);
        */
    }
    coro_transfer(&stuff[0].ctx, &stuff[1].ctx);
    ret = 0;
end:
    for (int i = 0; i < length; i++) {
        coro_destroy(&stuff[i].ctx);
        coro_stack_free(&stuff[i].stack);
    }
    return ret;
}
