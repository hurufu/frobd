#pragma once
#include "utils.h"

typedef void* (* const entry_t)(void*);

struct task;
union retval {
    void* ptr;
    int num;
};

#define create_task(Name, Entry, ...) create_task_(Name, Entry, &(struct Entry ## _args){ __VA_ARGS__ });
struct task* create_task_(const char* name, entry_t entry, const void* arg);
int teardown_task(struct task*);

size_t send_message(int fd, const input_t* p, const input_t* pe) __attribute__((nonnull(2,3)));
size_t recv_message(int fd, size_t size, input_t p[static size], const input_t** pe) __attribute__((nonnull(3,4)));

inline size_t send_message_buf(const int fd, const size_t size, const input_t buf[static const size]) {
    return send_message(fd, buf, buf + size);
}
