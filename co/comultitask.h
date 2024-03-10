#pragma once

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

enum fdt { FDT_READ, FDT_WRITE, FDT_EXCEPT };

typedef int (* sus_entry)(void*);

struct sus_coroutine_reg {
    const size_t stack_size;
    int result;
    const sus_entry entry;
    void* const args;
};

int sus_select(int n, fd_set* restrict r, fd_set* restrict w, fd_set* restrict e, struct timeval* restrict t);
int sus_notify(enum fdt set, int fd);
ssize_t sus_write(int fd, const void* data, size_t size);
ssize_t sus_read(int fd, void* data, size_t size);
void sus_lend(int id, void* data, size_t size);
ssize_t sus_borrow(int id, void** value);
void sus_return(const int id);
int sus_runall(size_t s, struct sus_coroutine_reg (* c)[s]);
int sus_wait(void);
