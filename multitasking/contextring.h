#pragma once

#include "coro/coro.h"

struct coro_context_ring {
    struct coro_context_ring* prev, * next;
    struct coro_context* const ctx;
    int visited;
    const char* const name;
};

void insert(struct coro_context_ring** cursor, struct coro_context* ctx, const char* name) __attribute__((nonnull (1,2)));
void shrink(struct coro_context_ring** cursor) __attribute__((nonnull (1)));
