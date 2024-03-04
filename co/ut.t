#include <check.h>
#include "contextring.h"
#include "../utils.h"

int g_log_level = LOG_DEBUG;

#suite context_ring

#test single_element_can_be_added_and_removed
    struct coro_context tmp = {};
    struct coro_context_ring* ring = NULL;
    insert(&ring, &tmp);
    ck_assert_ptr_nonnull(ring);
    ck_assert_ptr_eq(ring->ctx, &tmp);
    ck_assert_ptr_eq(ring->next, ring);
    ck_assert_ptr_eq(ring->prev, ring);
    shrink(&ring);
    ck_assert_ptr_null(ring);

#test multiple_elements_can_be_added_and_removed
    struct coro_context_ring* ring = NULL;
    for (int i = 1; i <= 1000; i++) {
        struct coro_context tmp[i] = {};
        for (size_t i = 0; i < lengthof(tmp); i++)
            insert(&ring, &tmp[i]);
        struct coro_context_ring* p = ring;
        for (size_t i = 0; i < lengthof(tmp); i++) {
            //ck_assert_ptr_eq(p->ctx, &tmp[i]);
            ck_assert_ptr_eq(p, p->next->prev);
            ck_assert_ptr_eq(p, p->prev->next);
        }
        for (size_t i = 0; i < lengthof(tmp); i++)
            shrink(&ring);
        ck_assert_ptr_null(ring);
    }
