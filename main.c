#include "frob.h"
#include <stdio.h>

int main() {
    ssize_t producer(unsigned char (*const b)[2 * 1024]) {
        return read(0, *b, sizeof(*b));
    }

    int consumer (const struct FrobMsg* const m) {
        puts("Ack");
        fwrite(m, sizeof(struct FrobMsg), 1, stdout);
        putchar('\n');
        return 0;
    }

    const ssize_t ret = frob_match(producer, consumer);
    perror("DONE");
    return ret ? 1 : 0;
}
