#include "frob.h"
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <err.h>

#define elementsof(Array) (sizeof(Array)/sizeof((Array)[0]))

int main() {
    static unsigned char buf[4096];
    const unsigned char* const end = buf + sizeof buf;

    {
        FILE* const ios[] = { stdin, stdout };
        for (size_t i = 0; i < elementsof(ios); i++)
            setbuf(ios[i], NULL);
    }

    for (;;) {
        struct frob_frame_fsm_state st = { .end = buf };
        goto again;
        switch (frob_frame_process(&st)) {
            case EBADMSG:
nak:
                if (fwrite((char[]){0x15}, 1, 1, stdout) != 1)
                    err(3, "NAK failed");
                continue;
            case EAGAIN:
again:
                st.start = st.end;
                const size_t s = fread(st.start, 1, end - st.start, stdin);
                if (s <= 0)
                    goto end;
                st.end = st.start + s;
                switch (frob_frame_process(&st)) {
                    case EBADMSG:
                        goto nak;
                    case EAGAIN:
                        goto again;
                }
        }
        if (fwrite((char[]){0x06}, 1, 1, stdout) != 1)
            err(2, "ACK failed");
    }
end:
    if (feof(stdin))
        return 0;
    err(5, "Can't read stdin");
}
