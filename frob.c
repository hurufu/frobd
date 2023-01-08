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

#define FS "\x1C"

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

struct preformated_messages {
    byte_t t2[32];
};

#ifndef NO_LOGS_ON_STDERR
enum LogLevel g_log_level = LOG_DEBUG;
#endif

static int handle_message(const struct preformated_messages* const pm, const struct frob_msg* const msg, int (*channel)[CHANNELS_COUNT], struct io_state* const t) {
    assertion("Magic string shall match", strcmp(msg->magic, "FROBCr1") == 0);
    switch (msg->header.type) {
        case FROB_T1:
            memcpy(t->cur[EW_MAIN], pm->t2, elementsof(pm->t2));
            t->cur[EW_MAIN] += elementsof(pm->t2);
            FD_SET((*channel)[EW_MAIN], &t->set[FD_WRITE]);
            break;
        default:
            LOGWX("Handling of this message type isn't implemented");
            return -1;
    }
    return 0;
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

static int process_msg(const unsigned char* p, const unsigned char* const pe, struct frob_msg* const msg) {
    msg->header = frob_header_extract(&p, pe);
    LOGDX("\tTYPE: %02X TOKEN: %02X %02X %02X",
            msg->header.type, msg->header.token[0], msg->header.token[1], msg->header.token[2]);

    static int protocol_state = -1;
    switch (frob_protocol_transition(&protocol_state, msg->header.type)) {
        case EPROTO:
            LOGDX("Out of order message");
            return -1;
    }

    switch (frob_body_extract(msg->header.type, &p, pe, &msg->body)) {
        case EBADMSG:
            LOGDX("Bad payload");
            return -1;
    }

    switch (frob_extract_additional_attributes(&p, pe, &msg->attr)) {
        case EBADMSG:
            LOGDX("Bad data");
            return -1;
    }

    assertion("Complete message shall be processed, ie cursor shall point to the end of message", p == pe);
    return 0;
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
    struct timeval* const tp = (relative_timeout == (time_t)-1) ? NULL : &t;
    // TODO: Use pselect and enable master channel on SIGINT
    const int l = select(nfd, &(*set)[FD_READ], &(*set)[FD_WRITE], &(*set)[FD_EXCEPT], tp);
    if (l < 0)
        LOGW("Select failed");
    else if (l == 0)
        if (relative_timeout)
            LOGWX("Timeout reached");
        else
            LOGWX("Single-shot mode ended");
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
                        // FIXME: Retry if we were unable to write all bytes
                        if ((s = write((*channel)[i], t->buf[i], t->cur[i] - t->buf[i])) != t->cur[i] - t->buf[i])
                            LOGF("Can't write %td bytes to %s channel (fd %d)", t->cur[i] - t->buf[i], channel_to_string(i), (*channel)[i]);
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

static struct frob_frame_fsm_state fnext(const byte_t* const cursor, const struct frob_frame_fsm_state prev) {
    return (struct frob_frame_fsm_state){
        .p = prev.pe + 2,
        .pe = cursor
    };
}

static int event_loop(const struct preformated_messages* const pm, int (* const channel)[CHANNELS_COUNT], const time_t relative_timeout) {
    static struct io_state t;

    const int m = get_max_fd(channel);
    struct frob_frame_fsm_state f = { .p = t.buf[ER_MAIN] };
    for (finit(channel, &t); fselect(m, &t.set, relative_timeout); fset(channel, &t.set)) {
        perform_pending_io(&t, channel);

        if (FD_ISSET((*channel)[ER_MAIN], &t.set[FD_READ])) {
            int e;
            for (f.pe = t.cur[ER_MAIN]; (e = frob_frame_process(&f)) != EAGAIN; f = fnext(t.cur[ER_MAIN], f)) {
                assertion("Message length shall be positive", f.pe > f.p);
                const byte_t ack[] = { e ? 0x15 : 0x06 };
                *t.cur[EW_MAIN]++ = ack[0];
                FD_SET((*channel)[EW_MAIN], &t.set[FD_WRITE]);
                char tmp[3*(f.pe-f.p)];
                LOGDX("%s → %s", PRETTV(f.p, f.pe, tmp), PRETTY(ack));
                if (0 == e) {
                    struct frob_msg parsed = { .magic = "FROBCr1" };
                    if (process_msg(f.p, f.pe, &parsed) != 0)
                        LOGWX("Can't process message");
                    else
                        if (handle_message(pm, &parsed, channel, &t) != 0)
                            LOGWX("Can't handle message");
                }
            }
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
    const struct preformated_messages pm = {
        .t2 = FS "T2" FS "170" FS "TEST" FS "SIM" FS "0" FS
    };
    const time_t timeout = 0;
    if (event_loop(&pm, &channel, timeout) == 0)
        return timeout ? EXIT_FAILURE : EXIT_SUCCESS;
}
