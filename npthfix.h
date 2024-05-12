#pragma once
#include <npth.h>
#include <pthread.h>

#define npth_define(Entry, Name, ...) \
    { .entry = (npth_entry_t)Entry, .name = (Name), .arg = &(struct Entry ## _args){ __VA_ARGS__ } }

typedef void* (* const npth_entry_t)(void*);
typedef void (* const cleanup_t)(void*);

struct ThreadBag {
    npth_t handle;
    const char* const name;
    npth_entry_t entry;
    void* const arg;
};

size_t xsend_message(int fd, const input_t* p, const input_t* pe) __attribute__((nonnull(2,3)));
size_t xrecv_message(int fd, size_t size, input_t p[static size], const input_t** pe) __attribute__((nonnull(3,4)));

inline size_t xsend_message_buf(const int fd, const size_t size, const input_t buf[static const size]) {
    return xsend_message(fd, buf, buf + size);
}
