#include "contextring.h"
#include "../utils.h"

void insert(struct coro_context_ring** const cursor, struct coro_context* const ctx) {
    struct coro_context_ring* const tmp = xmalloc(sizeof (struct coro_context_ring));
    if (*cursor) {
        *tmp = (struct coro_context_ring) {
            .ctx = ctx,
            .next = (*cursor)->next,
            .prev = *cursor
        };
        (*cursor)->next->prev = tmp;
        (*cursor)->next = tmp;
        *cursor = tmp;
    } else {
        *tmp = (struct coro_context_ring) {
            .ctx = ctx,
            .next = tmp,
            .prev = tmp
        };
    }
    *cursor = tmp;
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
