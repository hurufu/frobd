#include "frob.h"
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#define elementsof(Array) (sizeof(Array)/sizeof((Array)[0]))

static char* convert_to_printable(const unsigned char* const p, const unsigned char* const pe,
                                  const size_t s, char b[static const s]) {
    // TODO: Add support for regional characters, ie from 0x80 to 0xFF
    // TODO: Rewrite convert_to_printable using libicu
    static const char special[][4] = {
        "␀", "␁", "␂", "␃", "␄", "␅", "␆", "␇", "␈", "␉", "␊", "␋", "␌", "␍", "␎", "␏",
        "␐", "␑", "␒", "␓", "␔", "␕", "␖", "␗", "␘", "␙", "␚", "␛", "␜", "␝", "␞", "␟",
        "␠",
        [0x7F] = "␡",
        [0x80] = "␦"
    };
    char* o = b;
    for (const unsigned char* x = p; x != pe && o < b + s; x++) {
        const unsigned char c = (*x & 0x80) ? 0x80 : *x;
        if (c <= 0x20 || c == 0x7F || c == 0x80)
            for (int i = 0; i < 3; i++)
                *o++ = special[c][i];
        else
            *o++ = c;
    }
    *o = 0x00;
    return b;
}

static int frame_loop() {
    static unsigned char buf[4096];
    static const unsigned char* const end = buf + sizeof buf;
    for (;;) {
        char ack[] = { 0x06 };
        struct frob_frame_fsm_state st = { .pe = buf };
        goto again;
process:
        switch (frob_frame_process(&st)) {
            case EAGAIN:
again:
                st.p = st.pe;
                const size_t s = fread(st.p, 1, end - st.p, stdin);
                if (s <= 0)
                    goto end_read;
                st.pe = st.p + s;
                goto process;
            case EBADMSG:
                ack[0] = 0x15;
        }
        if (fwrite(ack, 1, sizeof ack, stdout) != 1)
            goto end_write;
        char buf[4 * (st.pe - st.p)];
        fprintf(stderr, "< %s\n", convert_to_printable(st.p, st.pe, sizeof buf, buf));
        fprintf(stderr, "> %s\n", convert_to_printable(ack, ack + sizeof ack, 4, (char[4]){}));
    }
end_read:
    if (feof(stdin))
        return 0;
    return 5;
end_write:
    if (feof(stdout))
        return 1;
    return 2;
}

int main() {
    {
        FILE* const ios[] = { stdin, stdout };
        for (size_t i = 0; i < elementsof(ios); i++)
            setbuf(ios[i], NULL);
    }

    return frame_loop();
}
