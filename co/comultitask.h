#pragma once

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

enum ioset { IOSET_READ, IOSET_WRITE, IOSET_OOB };

enum channel {
    CHANNEL_FI_MAIN = 0,
    CHANNEL_FO_MAIN = 1
};

typedef int (* sus_entry)(void*);

struct sus_coroutine_reg {
    const size_t stack_size;
    int result;
    const sus_entry entry;
    void* const args;
};

void sus_lend(int id, size_t size, void* data);
ssize_t sus_borrow(enum channel* id, void** value);
void sus_return(const int id);
int sus_runall(size_t s, struct sus_coroutine_reg (* c)[s]) __attribute__((nonnull (2)));
