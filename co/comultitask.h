#pragma once

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

enum ioset { IOSET_READ, IOSET_WRITE, IOSET_OOB };

typedef int (* sus_entry)(void*);

struct sus_coroutine_reg {
    const size_t stack_size;
    int result;
    const sus_entry entry;
    void* const args;
};

struct sus_args_io_loop {
    int s6_notification_fd;
    time_t timeout;
    unsigned routines;
};

ssize_t sus_read(int fd, void* data, size_t size) __attribute__((nonnull(2)));
ssize_t sus_write(int fd, void* data, size_t size) __attribute__((nonnull(2)));
int sus_io_loop(struct sus_args_io_loop* args) __attribute__((nonnull(1)));
void sus_lend(uint8_t ch, size_t size, void* data) __attribute__((nonnull(3)));
ssize_t sus_borrow(uint8_t id, void** value) __attribute__((nonnull(2)));
void sus_return(uint8_t id, const void* data, size_t size);

int sus_runall(size_t s, struct sus_coroutine_reg (* c)[s]) __attribute__((nonnull (2)));
