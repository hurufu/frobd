#include "sus.h"


struct Task {
    struct coro_stack stack;
    struct coro_context ctx;
};



static struct Task s_waiting;


int sus_create(const struct sus_registation_form r) {
    coro_stack_alloc(&stack, r.stack_size);
    coro_create(&ctx, starter, &r, stack.sptr, stack.ssze);
}
