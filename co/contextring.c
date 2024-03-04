#include "contextring.h"

void insert(struct coro_context_ring** const cursor, struct coro_context* const ctx) {
    (void)cursor, (void)ctx;
}

void shrink(struct coro_context_ring** const cursor) {
    (void)cursor;
}
