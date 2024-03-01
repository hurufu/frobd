#include "comultitask.h"
#include "../coro/coro.h"
#include "../log.h"
#include "../utils.h"
#include <assert.h>
#include <string.h>
#include <unistd.h>

struct coro_context_ring {
    struct coro_context_ring* prev, * next;
    coro_context* ctx;
};

struct coro_stuff {
    struct coro_stack stack;
    coro_context ctx;
};

static struct coro_context_ring* s_current, * s_end;

static size_t fd_to_index(const int fd) {
    switch (fd) {
        case STDIN_FILENO:
            return 1;
        case STDOUT_FILENO:
            return 2;
    }
    return 0;
}

static size_t indexof(const uint8_t set, const int fd) {
    return set * fd_to_index(fd);
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



/** Transfer to a coroutine that is waiting on a file descriptor.
 *
 *  @param [in] set 0-2 (read, write, oob)
 *  @param [in] fd file descriptor in set
 */
int sus_resume(const uint8_t set, const int fd) {
#   if 0
    coro_context* const origin = s_current, * const destination = s_current + indexof(set, fd);
    if (destination < s_current)
        return -1;
    coro_transfer(origin, destination);
#   endif
}

// Transfer to scheduler and place current coroutine to a pending list
void sus_yield(void) {
#   if 0
    coro_context* const origin = s_current, * const next = NULL /* ... */;
    s_current = next;
    coro_transfer(origin, next);
#   endif
}

int sus_wait(void) {
    return 0;
}

// Transfer to scheduler and forget about current coroutine
__attribute__((noreturn))
static inline void sus_return(void) {
#   if 0
    bool no_more_left = false;
    coro_context* const origin = s_current->ctx, * const destination = no_more_left ? s_end : s_current->next->ctx;
    s_current->prev->next = s_current->next;
    free(s_current);
    coro_transfer(origin, destination);
#   endif
    assert(0);
}

__attribute__((noreturn))
static void starter(struct sus_coroutine_reg* const reg) {
    reg->result = reg->entry(reg->args);
    sus_return();
}

int sus_jumpstart(const size_t length, struct sus_coroutine_reg h[static const length]) {
    assert(h);
    int ret = -1;
    struct coro_stuff stuff[length + 1];
    memset(stuff, 0, sizeof stuff);
    coro_create(&stuff[0].ctx, NULL, NULL, NULL, 0);
    for (int i = 0; i < length; i++) {
        if (!coro_stack_alloc(&stuff[i+1].stack, h[i].stack_size))
            goto end;
        coro_create(&stuff[i+1].ctx, (void(*)(void*))starter, &h[i], stuff[i+1].stack.sptr, stuff[i+1].stack.ssze);
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
