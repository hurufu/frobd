#include "frob.h"
#include "utils.h"
#include "log.h"

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <assert.h>
#include <poll.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

/* uint8_t is better than unsigned char to define a byte, because on some
 * platforms (unsigned) char may have more than 8 bit (TI C54xx 16 bit).
 * The problem is purely theoretical, because I don't target MCUs, but still...
 */
typedef uint8_t byte_t;

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

struct io_state {
    fd_set set[FD_SET_COUNT];
    byte_t* cur[CHANNELS_COUNT];
    byte_t buf[CHANNELS_COUNT][4 * 1024];
};


enum LogLevel g_log_level = LOG_DEBUG;

int frob_forward_msg(const struct frob_msg* const msg) {
    assertion("Magic string shall match", strcmp(msg->magic, "FROBCr1") == 0);
    LOGIX("Not implemented");
    return -1;
};

static const char* channel_to_string(const enum OrderedChannels o) {
    switch (o) {
        case IW_PAYMENT: return "IW_PAYMENT";
        case IW_STORAGE: return "IW_STORAGE";
        case IW_UI:      return "IW_UI";
        case IW_EVENTS:  return "IW_EVENTS";
        case IR_DEVICE:  return "IR_DEVICE";
        case EW_MAIN:    return "EW_MAIN";
        case ER_MAIN:    return "ER_MAIN";
        case ER_MASTER:  return "ER_MASTER";
        case CHANNELS_COUNT:
    }
    return NULL;
}

static void process_msg(const unsigned char* p, const unsigned char* const pe) {
    struct frob_msg msg = {
        .magic = "FROBCr1",
        .header = frob_header_extract(&p, pe)
    };
    LOGDX("\tTYPE: %02X TOKEN: %02X %02X %02X",
            msg.header.type, msg.header.token[0], msg.header.token[1], msg.header.token[2]);

    static int protocol_state = -1;
    switch (frob_protocol_transition(&protocol_state, msg.header.type)) {
        case EPROTO:
            LOGDX("Out of order message");
            return;
    }

    switch (frob_body_extract(msg.header.type, &p, pe, &msg.body)) {
        case EBADMSG:
            LOGDX("Bad payload");
            return;
    }

    switch (frob_extract_additional_attributes(&p, pe, &msg.attr)) {
        case EBADMSG:
            LOGDX("Bad data");
            return;
    }

    assertion("Complete message shall be processed, ie cursor shall point to the end of message", p == pe);

    if (frob_forward_msg(&msg) != 0)
        LOGDX("Downstream error");
}

static void fset(const int (* const channel)[CHANNELS_COUNT], fd_set (* const set)[FD_SET_COUNT]) {
    for (size_t i = 0; i < CHANNELS_COUNT; i++) {
        const int c = (*channel)[i];
        if (c != -1) {
            FD_SET(c, &(*set)[FD_EXCEPT]);
            switch (i) {
                case ER_MAIN:
                case IR_DEVICE:
                case ER_MASTER:
                    FD_SET(c, &(*set)[FD_READ]);
            }
        }
    }
}

static void finit(const int (* const channel)[CHANNELS_COUNT], struct io_state* const t) {
    for (size_t i = 0; i < elementsof(t->cur); i++)
        t->cur[i] = t->buf[i];
    for (size_t i = 0; i < elementsof(t->set); i++)
        FD_ZERO(&t->set[i]);
    fset(channel, &t->set);
}

static bool fselect(const int nfd, fd_set (* const set)[FD_SET_COUNT], const time_t relative_timeout) {
    struct timeval t = { .tv_sec = relative_timeout };
    // TODO: Use pselect and enable master channel on SIGINT
    const int l = select(nfd, &(*set)[FD_READ], &(*set)[FD_WRITE], &(*set)[FD_EXCEPT], &t);
    if (l < 0)
        LOGW("Select failed");
    else if (l == 0)
        LOGWX("Timeout reached");
    return l;
}

static int get_max_fd(const int (*channel)[CHANNELS_COUNT]) {
    int max = (*channel)[0];
    for (size_t i = 1; i < elementsof(*channel); i++)
        max = max < (*channel)[i] ? (*channel)[i] : max;
    return max + 1;
}

static bool all_channels_ok(const int (*channel)[CHANNELS_COUNT]) {
    for (size_t i = 0; i < elementsof(*channel); i++)
        if ((*channel)[i] >= FD_SETSIZE)
            return false;
    return true;
}

static void perform_pending_io(struct io_state* const t, int (*channel)[CHANNELS_COUNT]) {
    for (size_t j = 0; j < elementsof(t->set); j++) {
        for (size_t i = 0; i < elementsof(*channel); i++) {
            if (FD_ISSET((*channel)[i], &(t->set)[j])) {
                switch (j) {
                    ssize_t s;
                    case FD_EXCEPT:
                        LOGFX("Exceptional data isn't supported");
                        break;
                    case FD_WRITE:
                        if ((s = write((*channel)[i], t->buf[i], t->cur[i] - t->buf[i])) != t->cur[i] - t->buf[i])
                            LOGF("Can't write %zu bytes to %s channel (fd %d)", t->cur[i] - t->buf[i], channel_to_string(i), (*channel)[i]);
                        t->cur[i] = t->buf[i];
                        FD_CLR((*channel)[i], &(t->set)[FD_WRITE]);
                        break;
                    case FD_READ:
                        if ((s = read((*channel)[i], t->cur[i], t->buf[i] + sizeof t->buf[i] - t->cur[i])) < 0) {
                            LOGF("Can't read data on %s channel (fd %d)", channel_to_string(i), (*channel)[i]);
                        } else if (s == 0) {
                            LOGIX("Channel %s (fd %d) was closed", channel_to_string(i), (*channel)[i]);
                            close((*channel)[i]);
                            FD_CLR((*channel)[i], &(t->set)[FD_READ]);
                            (*channel)[i] = -1;
                        }
                        t->cur[i] += s;
                        break;
                }
            }
        }
    }
}

static int event_loop(int (* const channel)[CHANNELS_COUNT], const time_t relative_timeout) {
    static struct io_state t;

    const int m = get_max_fd(channel);
    struct frob_frame_fsm_state f = { .p = t.buf[ER_MAIN] };
    for (finit(channel, &t); fselect(m, &t.set, relative_timeout); fset(channel, &t.set)) {
        perform_pending_io(&t, channel);

        if (FD_ISSET((*channel)[ER_MAIN], &t.set[FD_READ])) {
            f.pe = t.cur[ER_MAIN];
            const int e = frob_frame_process(&f);
            if (e == EAGAIN) {
                f.p = f.pe;
                continue;
            }
            const byte_t ack[] = { e ? 0x15 : 0x06 };
            if (write((*channel)[EW_MAIN], ack, sizeof ack) != sizeof ack)
                return -15;

            {
                char tmp[3*(f.pe-f.p)];
                LOGDX("%s → %s", PRETTV(f.p, f.pe, tmp), PRETTY(ack));
            }

            if (e == 0)
                process_msg(f.p, f.pe);

            f.pe = f.p = t.buf[ER_MAIN];
        }
    }

    return 0;
}

int main() {
    int channel[] = {
        [IW_PAYMENT] = -1,
        [IW_STORAGE] = -1,
        [IW_UI     ] = -1,
        [IW_EVENTS ] = -1,
        [IR_DEVICE ] = -1,
        [EW_MAIN   ] = STDOUT_FILENO,
        [ER_MAIN   ] = STDIN_FILENO,
        [ER_MASTER ] = -1
    };

    if (!all_channels_ok(&channel))
        return EXIT_FAILURE;

    event_loop(&channel, 1);
    return EXIT_FAILURE;
}
