#include "frob.h"
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#define elementsof(Array) (sizeof(Array)/sizeof((Array)[0]))

int main() {
    static unsigned char buf[4096];
    static const unsigned char* const end = buf + sizeof buf;

    {
        FILE* const ios[] = { stdin, stdout };
        for (size_t i = 0; i < elementsof(ios); i++)
            setbuf(ios[i], NULL);
    }

    for (;;) {
        char ack = 0x06;
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
                ack = 0x15;
        }
        if (fwrite(&ack, 1, 1, stdout) != 1)
            goto end_write;
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
