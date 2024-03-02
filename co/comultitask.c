#include "comultitask.h"
#include "../coro/coro.h"
#include "../log.h"
#include "../utils.h"
#include <assert.h>
#include <string.h>
#include <unistd.h>

struct coro_stuff {
    struct coro_stack stack;
    struct coro_context ctx;
};

struct coro_context_ring {
    struct coro_context_ring* prev, * next;
    struct coro_context* ctx;
};

static struct coro_context s_end;
static struct coro_context_ring* s_current;
static int s_current_fd[3] = { -1, -1, -1 };

static void suspend(void) {
    coro_transfer(s_current->ctx, &s_end);
}

static void suspend_until_fd(const enum fdt first, const enum fdt last, const int fd) {
    for (enum fdt set = first; set <= last; set++)
        while (fd != s_current_fd[set])
            suspend();
    if (first == last)
        s_current_fd[first] = -1;
}

ssize_t sus_write(const int fd, const void* const data, const size_t size) {
    LOGDX("> %d", fd);
    suspend_until_fd(FDT_WRITE, FDT_WRITE, fd);
    const ssize_t ret = write(fd, data, size);
    LOGDX("< %d", fd);
    return ret;
}

ssize_t sus_read(const int fd, void* const data, const size_t size) {
    LOGDX("> %d", fd);
    suspend_until_fd(FDT_READ, FDT_READ, fd);
    const ssize_t ret = read(fd, data, size);
    LOGDX("< %d", fd);
    return ret;
}

int sus_select(const int n, fd_set* restrict r, fd_set* restrict w, fd_set* restrict e, struct timeval* restrict t) {
    LOGDX(">");
    suspend_until_fd(FDT_READ, FDT_EXCEPT, -1);
    const int ret = select(n, r, w, e, t);
    LOGDX("<");
    return ret;
}

// Wait untile connected coroutine yields
ssize_t sus_lend(const int id, void* const data, const size_t size) {
    return size;
}

// Transfer directly to connected coroutine
ssize_t sus_borrow(const int id, void** const data) {
    return -1;
}

int sus_resume(const enum fdt set, const int fd) {
    LOGDX("%d %d", set, fd);
    s_current_fd[set] = fd;
    suspend();
}

// Transfer to scheduler and forget about current coroutine
__attribute__((noreturn))
static inline void sus_return(void) {
    coro_context* const origin = s_current->ctx;
    s_current->prev->next = s_current->next;
    s_current->next->prev = s_current->prev;
    s_current = s_current == s_current->next ? NULL : s_current->next;
    LOGDX("%p", s_current);
    //free(s_current);
    coro_transfer(origin, &s_end);
    assert(0);
}

__attribute__((noreturn))
static void starter(struct sus_coroutine_reg* const reg) {
    reg->result = reg->entry(&reg->ca, reg->args);
    sus_return();
}

int sus_jumpstart(const size_t length, struct sus_coroutine_reg (* const h)[length]) {
    assert(h);
    int ret = -1;
    struct coro_stuff stuff[length];
    memset(stuff, 0, sizeof stuff);

    coro_create(&s_end, NULL, NULL, NULL, 0);

    for (int i = 0; i < length; i++) {
        if (!coro_stack_alloc(&stuff[i].stack, (*h)[i].stack_size))
            goto end;
        coro_create(&stuff[i].ctx, (coro_func)starter, &(*h)[i], stuff[i].stack.sptr, stuff[i].stack.ssze);
    }

    s_current = malloc(sizeof (struct coro_context_ring));
    *s_current = (struct coro_context_ring) {
        .ctx = &stuff[0].ctx,
        .prev = s_current,
        .next = s_current
    };
    for (int i = 1; i < length; i++) {
        struct coro_context_ring* const next = malloc(sizeof (*s_current));
        *next = (struct coro_context_ring){
            .ctx = &stuff[i].ctx,
            .prev = s_current,
            .next = s_current->next
        };
        s_current->next = next;
    }

    int i = 0;
    for (; s_current; s_current = s_current->next) {
        LOGDX(". %d %p", i++, s_current);
        coro_transfer(&s_end, s_current->ctx);
        LOGDX(":");
    }

    LOGDX("end");
    ret = 0;
end:
    /*
    s_current->prev->next = NULL; // Break the cycle
    for (struct coro_context_ring* c = s_current; c;) {
        struct coro_context_ring* const next = c->next;
        //free(c);
        c = next;
    }
    */
    for (int i = 0; i < length; i++) {
        coro_destroy(&stuff[i].ctx);
        coro_stack_free(&stuff[i].stack);
    }
    s_current = NULL;
    return ret;
}
