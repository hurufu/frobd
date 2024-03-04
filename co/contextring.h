#pragma once

#include "../coro/coro.h"

struct coro_context_ring {
    struct coro_context_ring* prev, * next;
    struct coro_context* ctx;
};

void insert(struct coro_context_ring** cursor, struct coro_context* ctx);
void shrink(struct coro_context_ring** cursor);
