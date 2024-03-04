#pragma once

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

enum fdt { FDT_READ, FDT_WRITE, FDT_EXCEPT };

struct coro_args {
    const int* fd, * idx;
};

typedef int (* sus_entry)(const struct coro_args*, void*);

struct sus_coroutine_reg {
    const size_t stack_size;
    struct coro_args ca;
    int result;
    const sus_entry entry;
    void* const args;
};

int sus_select(int n, fd_set* restrict r, fd_set* restrict w, fd_set* restrict e, struct timeval* restrict t);
ssize_t sus_write(int fd, const void* data, size_t size);
ssize_t sus_read(int fd, void* data, size_t size);
ssize_t sus_lend(int id, void* data, size_t size);
ssize_t sus_borrow(int id, void** value);
int sus_resume(enum fdt set, int fd);
int sus_runall(size_t s, struct sus_coroutine_reg (* c)[s]);
int sus_wait(void);
ssize_t sus_sendmsg(int mux, int priority, int type, size_t length, char value[static length]);
ssize_t sus_recvmsg(int mux, int* type, char** value);
void sus_yield(void);
