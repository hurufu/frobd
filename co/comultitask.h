#pragma once

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

enum ioset { IOSET_READ, IOSET_WRITE, IOSET_OOB };

struct iowork {
    int id;
    size_t size;
    void* data;
};

typedef int (* sus_entry)(void*);

struct sus_coroutine_reg {
    const size_t stack_size;
    int result;
    const sus_entry entry;
    void* const args;
};

void sus_lend(int ch, size_t size, void* data);

void sus_borrow_any(struct iowork*);
void sus_borrow_any_confirm(int id);

ssize_t sus_borrow(int id, void** value);

// Dual of sus_borrow
void sus_return(const int id);

int sus_runall(size_t s, struct sus_coroutine_reg (* c)[s]) __attribute__((nonnull (2)));
