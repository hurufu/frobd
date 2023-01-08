#include "frob.h"
#include "utils.h"
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <assert.h>
#include <poll.h>
#include <err.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>

/* uint8_t is better than unsigned char if char has more than 8 bit,
 * eg. TI C54xx, it's only theoretical problem, but still...
 */
typedef uint8_t byte_t;

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
    struct frob_msg msg = {
        .magic = "FROBCr1",
        .header = frob_header_extract(&p, pe)
    };
    fprintf(stderr, "\tTYPE: %02X TOKEN: %02X %02X %02X\n",
            msg.header.type, msg.header.token[0], msg.header.token[1], msg.header.token[2]);

    static int protocol_state = -1;
    switch (frob_protocol_transition(&protocol_state, msg.header.type)) {
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

int _main() {
    FILE* const ios[] = { stdin, stdout };
    for (size_t i = 0; i < elementsof(ios); i++)
        setbuf(ios[i], NULL);

    return frame_loop();
}

// e – external, i - internal, m – manual
// r - read/input, w - write/output
enum OrderedChannels {
    IW_PAYMENT,
    IW_STORAGE,
    IW_UI,
    IW_EVENTS,
    IR_DEVICE,
    EW_MAIN,
    ER_MAIN,
    ER_MASTER,

    CHANNELS_COUNT
};

enum OrderedFdSets {
    FD_EXCEPT,
    FD_WRITE,
    FD_READ,

    FD_SET_COUNT
};

struct params {
    int channel[CHANNELS_COUNT];
    time_t timeout;
};

int _loop(const struct params* const a) {
    static byte_t buffer[elementsof(a->channel)][4096], * cursor[elementsof(buffer)];

    // Validate file descriptors and select the highest one
    const int maxfd = ({
        int max = a->channel[0];
        for (size_t i = 1; i < elementsof(a->channel); i++)
            if (a->channel[i] >= FD_SETSIZE)
                return EINVAL;
            else if (a->channel[i] > max)
                max = a->channel[i];
        max;
    });

    // Reset each cursor
    for (size_t i = 0; i < elementsof(cursor); i++)
        cursor[i] = buffer[i];

    fd_set fdset[FD_SET_COUNT];
    for (size_t i = 0; i < elementsof(fdset); i++)
        FD_ZERO(&fdset[i]);

    int r;
    struct timeval timeout;
    struct frob_frame_fsm_state main_fsm = { .pe = buffer[ER_MAIN] };

    goto setup;

    while ((r = select(maxfd, &fdset[FD_READ], &fdset[FD_WRITE], &fdset[FD_EXCEPT], &timeout)) > 0) {
        // Perform actual I/O for each selected channel of every file descriptor set
        for (size_t j = 0; j < elementsof(fdset); j++)
            for (size_t i = 0; i < elementsof(a->channel); i++)
                if (FD_ISSET(a->channel[i], &fdset[j]))
                    switch (j) {
                        ssize_t s;
                        case FD_EXCEPT:
                            return -2;
                        case FD_WRITE:
                            if ((s = write(a->channel[i], buffer[i], cursor[i] - buffer[i])) != cursor[i] - buffer[i])
                                return -4;
                            cursor[i] = buffer[i];
                            FD_CLR(a->channel[i], &fdset[FD_WRITE]);
                            break;
                        case FD_READ:
                            if ((s = read(a->channel[i], cursor[i], buffer[i] + sizeof buffer[i] - cursor[i])) <= 0)
                                return -3;
                            cursor[i] += s;
                            break;
                    }

        // Process received data on main channel
        if (FD_ISSET(a->channel[ER_MAIN], &fdset[FD_READ])) {
            main_fsm.pe = cursor[ER_MAIN];
            switch (frob_frame_process(&main_fsm)) {
                case EAGAIN:
                    break;
                case EBADMSG:
                    write(a->channel[EW_MAIN], (byte_t[]){ 0x15 }, 1);
                    break;
                case 0:
                    write(a->channel[EW_MAIN], (byte_t[]){ 0x06 }, 1);
                    process_msg(main_fsm.p, main_fsm.pe);
                    main_fsm.pe = buffer[ER_MAIN];
                    break;
            }
        }

setup:
        // (Re-) Initialize parameters for select
        main_fsm.p = main_fsm.pe;
        timeout = (struct timeval){ .tv_sec = a->timeout };
        for (size_t i = 0; i < elementsof(a->channel); i++) {
            const int c = a->channel[i];
            if (c != -1) {
                FD_SET(c, &fdset[FD_EXCEPT]);
                if (i == ER_MAIN || i == IR_DEVICE || i == ER_MASTER)
                    FD_SET(c, &fdset[FD_READ]);
            }
        }
    }

    return r;
}

int main() {
    const struct params pr = {
        .channel = {
            [IW_PAYMENT] = -1,
            [IW_STORAGE] = -1,
            [IW_UI     ] = -1,
            [IW_EVENTS ] = -1,
            [IR_DEVICE ] = -1,
            [EW_MAIN   ] = STDOUT_FILENO,
            [ER_MAIN   ] = STDIN_FILENO,
            [ER_MASTER ] = -1
        },
        .timeout = 1
    };
    err(_loop(&pr), "Exit");
}
