#include "frob.h"
#include "utils.h"
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <assert.h>

int frob_forward_msg(const struct frob_msg* const msg) {
    // Not implemented;
    return -1;
};

static char* convert_to_printable(const unsigned char* const p, const unsigned char* const pe,
                                  const size_t s, char b[static const s]) {
    // TODO: Add support for regional characters, ie from 0x80 to 0xFF
    // TODO: Rewrite convert_to_printable using libicu
    char* o = b;
    for (const unsigned char* x = p; x != pe && o < b + s; x++) {
        const unsigned char c = *x;
        if (c <= 0x20) {
            *o++ = 0xE2;
            *o++ = 0x90;
            *o++ = 0x80 + c;
        } else if (c == 0x7F) {
            *o++ = 0xE2;
            *o++ = 0x90;
            *o++ = 0x80 + 0x31;
        } else if (c & 0x80) {
            *o++ = 0xE2;
            *o++ = 0x90;
            *o++ = 0x36;
        } else {
            *o++ = c;
        }
    }
    *o = 0x00;
    return b;
}

static int process_msg(const unsigned char* p, const unsigned char* const pe) {
    struct frob_msg msg;

    msg.header = frob_header_extract(&p, pe);
    fprintf(stderr, "\tTYPE: %02X TOKEN: %02X %02X %02X\n",
            msg.header.type, msg.header.token[0], msg.header.token[1], msg.header.token[2]);

    static int ps = -1;
    switch (frob_protocol_transition(&ps, msg.header.type)) {
        case EPROTO:
            fprintf(stderr, "Out of order message\n");
            return 1;
    }

    switch (frob_body_extract(msg.header.type, &p, pe, &msg.body)) {
        case EBADMSG:
            fprintf(stderr, "Bad payload");
            return 2;
    }

    switch (frob_extract_additional_attributes(&p, pe, &msg.attr)) {
        case EBADMSG:
            fprintf(stderr, "Bad data");
            return 3;
    }

    assert(p == pe);

    if (frob_forward_msg(&msg) != 0) {
        fprintf(stderr, "Downstream error");
        return 4;
    }

    return 0;
}

static int frame_loop() {
    static unsigned char buf[4096];
    static const unsigned char* const end = buf + sizeof buf;
    for (;;) {
        unsigned char ack[] = { 0x06 };
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
        fprintf(stderr, "< %s\n> %s\n",
                convert_to_printable(st.p, st.pe, sizeof buf, buf),
                convert_to_printable(ack, ack + sizeof ack, 4, (char[4]){}));
        process_msg(st.p, st.pe);
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
    FILE* const ios[] = { stdin, stdout };
    for (size_t i = 0; i < elementsof(ios); i++)
        setbuf(ios[i], NULL);

    return frame_loop();
}