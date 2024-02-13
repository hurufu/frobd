#pragma once

#include <stddef.h>
#include <sys/types.h>

#define yield sus_yield()

typedef int (*sus_entry)(void*);

struct sus_coroutine_reg {
    const size_t stack_size;
    int result;
    const sus_entry entry;
    void* const args;
};

ssize_t sus_write(int fd, const void* data, size_t size);
ssize_t sus_read(int fd, void* data, size_t size);
ssize_t sus_lend(int id, void* data, size_t size);
ssize_t sus_borrow(int id, void** value);
int sus_resume(int set, int fd);
int sus_jumpstart(size_t s, struct sus_coroutine_reg c[static s]);
int sus_wait(void);
ssize_t sus_sendmsg(int mux, int priority, int type, size_t length, char value[static length]);
ssize_t sus_recvmsg(int mux, int* type, char** value);
void sus_yield(void);
