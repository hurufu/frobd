#pragma once
#include <npth.h>

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

size_t xnpth_write_fancy(int fd, size_t size, const unsigned char buf[static size]);
size_t xnpth_read_fancy(int fd, size_t size, unsigned char buf[static size]);
