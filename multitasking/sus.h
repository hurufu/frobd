#pragma once

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#include <time.h>

#define sus_registration(Entry, ...) (struct sus_registation_form){\
    .name = #Entry,\
    .stack_size = 0,\
    .entry = (sus_entry)Entry,\
    .args = &(struct Entry ## _args){ __VA_ARGS__ }\
}

typedef int (* sus_entry)(const void*);

struct sus_registation_form {
    const char* const name;
    const size_t stack_size;
    int result;
    const sus_entry entry;
    const void* const args;
};

struct sus_ioloop_args {
    const time_t timeout;
};

/** Start and schedule all tasks.
 *
 *  Currently uses round-robin scheduling scheme. In well-written program order
 *  of tasks in the input array shouldn't matter.
 *
 *  Another idea is to reorder tasks to improve shceduling
 */
int sus_runall(size_t s, struct sus_registation_form (* c)[s]) __attribute__((nonnull (2)));

ssize_t sus_read(int fd, void* data, size_t size) __attribute__((nonnull(2)));
ssize_t sus_write(int fd, const void* data, size_t size) __attribute__((nonnull(2)));
int sus_ioloop(struct sus_ioloop_args* args) __attribute__((nonnull(1)));

void sus_lend(uint8_t ch, size_t size, void* data) __attribute__((nonnull(3)));
ssize_t sus_borrow(uint8_t id, void** value) __attribute__((nonnull(2)));
void sus_return(uint8_t id, const void* data, size_t size);
void sus_disable(uint8_t id);
