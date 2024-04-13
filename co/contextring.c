#include "contextring.h"
#include "../utils.h"

void insert(struct coro_context_ring** const cursor, struct coro_context* const ctx, const char* const name) {
    assert(ctx);
    assert(cursor);
    struct coro_context_ring* const new = xmalloc(sizeof (struct coro_context_ring));
    memset(new, 0, sizeof *new);
    if (*cursor) {
        const struct coro_context_ring tmp = {
            .name = name,
            .ctx = ctx,
            .next = (*cursor)->next,
            .prev = *cursor
        };
        memcpy(new, &tmp, sizeof tmp);
        (*cursor)->next->prev = new;
        (*cursor)->next = new;
        *cursor = new;
    } else {
        const struct coro_context_ring tmp = {
            .name = name,
            .ctx = ctx,
            .next = new,
            .prev = new
        };
        memcpy(new, &tmp, sizeof tmp);
    }
    *cursor = new;
}

void shrink(struct coro_context_ring** const cursor) {
    assert(cursor && *cursor);
    if ((*cursor)->next == (*cursor)) {
        *cursor = NULL;
    } else {
        (*cursor)->prev->next = (*cursor)->next;
        (*cursor)->next->prev = (*cursor)->prev;
        *cursor = (*cursor)->next;
    }
    // FIXME: Free memory!
}
