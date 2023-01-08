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
#define ETX "\x03"

/* uint8_t is better than unsigned char to define a byte, because on some
 * platforms (unsigned) char may have more than 8 bit (TI C54xx 16 bit).
 * The problem is purely theoretical, because I don't target MCUs, but still...
 */
typedef uint8_t byte_t;

// e – external, i - internal, m – manual
// r - read/input, w - write/output
// Must be positive, because value is getting mixed with negative error codes
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

static const char* channel_to_string(const enum OrderedChannels o) {
    switch (o) {
        case IW_PAYMENT: return "PAYMENT (internal output)";
        case IW_STORAGE: return "STORAGE (internal output)";
        case IW_UI:      return "UI (internal output)";
        case IW_EVENTS:  return "EVENTS (internal output)";
        case IR_DEVICE:  return "DEVICE (internal input)";
        case EW_MAIN:    return "MAIN (external output)";
        case ER_MAIN:    return "MAIN (external input)";
        case ER_MASTER:  return "MASTER (console input)";
        case CHANNELS_COUNT:
    }
    return NULL;
}

static int forward_message(const struct frob_msg* const msg, const int channel, struct io_state* const t) {
    if (channel < 0) {
        return LOGWX("%s", strerror(EBADF));
    }
    if (t->cur[channel] + sizeof msg >= t->buf[channel] + sizeof t->buf[channel]) {
        return LOGWX("%s", strerror(ENOBUFS));
    }
    memcpy(t->cur[channel], msg, sizeof msg);
    t->cur[channel] += sizeof msg;
    FD_SET(channel, &t->set[FD_WRITE]);
    return 0;
}

static int process_msg(const unsigned char* p, const unsigned char* const pe, struct frob_msg* const msg) {
    msg->header = frob_header_extract(&p, pe);
    if (msg->header.type == 0)
        return LOGWX("Bad header");

    switch (frob_body_extract(msg->header.type, &p, pe, &msg->body)) {
        case EBADMSG:
            return LOGDX("Bad payload");
    }

    switch (frob_extract_additional_attributes(&p, pe, &msg->attr)) {
        case EBADMSG:
            return LOGDX("Bad data");
    }

    assertion("Complete message shall be processed, ie cursor shall point to the end of message", p == pe);
    return 0;
}

static const byte_t* get_preformatted_buffer(const struct preformated_messages* const pm, const enum FrobMessageType t) {
    switch (t) {
        case FROB_T1: return pm->t2;
        default: return NULL;
    }
}

static int make_frame(const byte_t* const body, const byte_t (* const token)[3], byte_t** const pp, byte_t* const pe) {
    byte_t* p = *pp;

    assert(pe >= p && pe - p <= 4*1024);

    const size_t l = strlen((char*)body);
    if (pe - *pp < 1 + 6 + (int)l + 1) // STX + token + body + LRC
        return LOGWX("Can't construct frame: %s", strerror(ENOBUFS));

    const int r = snprintf((char*)p, pe - p, "\x02%02X%02X%02X", (*token)[0], (*token)[1], (*token)[2]);
    if (r != 7)
        return LOGW("Can't construct token");
    p += r;
    memcpy(p, body, l);
    p += l;
    uint8_t lrc = 0;
    for (const byte_t* c = *pp + 1; c <= p; c++)
        lrc ^= *c;
    *p++ = lrc;

    assert(pe >= p && pe - p <= 4*1024);
    assertion("Parseable frame is created", frob_frame_process(&(struct frob_frame_fsm_state){.p = *pp, .pe = p}) == 0);
    assertion("Parseable message within is created", process_msg(*pp + 1, p - 2, &(struct frob_msg){ .magic = FROB_MAGIC }) == 0);

    *pp = p;
    return 0;
}

static int handle_local(const struct preformated_messages* const pm, const struct frob_header* const h, int (*channel)[CHANNELS_COUNT], struct io_state* const t) {
    const byte_t* const m = get_preformatted_buffer(pm, h->type);
    if (!m)
        return LOGWX("There isn't any anwer to reply: %s", strerror(ENOSYS));
    if (make_frame(m, &h->token, &t->cur[EW_MAIN], endof(t->buf[EW_MAIN])) != 0)
        return LOGWX("Locally generate response skipped");
    FD_SET((*channel)[EW_MAIN], &t->set[FD_WRITE]);
    return 0;
}

static int get_channel(const struct frob_msg* const msg) {
    switch (msg->header.type & FROB_DESTINATION_MASK) {
        case FROB_PAYMENT:
            if (msg->header.type == FROB_S1)
                switch (msg->body.s1.transaction_type) {
                    case FROB_TRANSACTION_TYPE_PAYMENT:
                        return IW_PAYMENT;
                    case FROB_TRANSACTION_TYPE_VOID:
                        return IW_STORAGE;
                }
            break;
        case FROB_UI: return IW_UI;
        case FROB_EVENT: return IW_EVENTS;
        case FROB_LOCAL: return -1;
        default:
            break;
    }
    return assertion("All channels must be handled", false), -2;
}

static int handle_message(const struct preformated_messages* const pm, const struct frob_msg* const msg, int (*channel)[CHANNELS_COUNT], struct io_state* const t) {
    assertion("Magic string shall match", strcmp(msg->magic, FROB_MAGIC) == 0);
    const int ch = get_channel(msg);
    if (ch == -2)
        return -2;
    if (ch == -1)
        return handle_local(pm, &msg->header, channel, t);
    return forward_message(msg, *(channel)[ch], t);
};

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
    if (l == 0)
        if (relative_timeout)
            LOGWX("Timeout reached");
        else
            LOGWX("Single-shot mode ended");
    else if (l < 0)
        LOGW("Select failed");
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
                        if ((s = write((*channel)[i], t->buf[i], t->cur[i] - t->buf[i])) != t->cur[i] - t->buf[i]) {
                            LOGF("Can't write %td bytes to %s channel (fd %d)", t->cur[i] - t->buf[i], channel_to_string(i), (*channel)[i]);
                        } else {
                            char tmp[3*s];
                            LOGDX("← %zu\t%s", s, PRETTV(t->buf[i], t->cur[i], tmp));
                        }
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
                        } else {
                            char tmp[3*s];
                            LOGDX("→ %zu\t%s", s, PRETTV(t->cur[i], t->cur[i] + s, tmp));
                        }
                        t->cur[i] += s;
                        break;
                }
            }
        }
    }
}

static struct frob_frame_fsm_state fnext(byte_t* const cursor, const struct frob_frame_fsm_state prev) {
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

        // TODO: Check if we have sent message on a main channel, and if so check for ACK/NAK from the other side.
        // TODO: Retransmit message when needed

        if (FD_ISSET((*channel)[ER_MAIN], &t.set[FD_READ])) {
            int e;
            for (f.pe = t.cur[ER_MAIN]; (e = frob_frame_process(&f)) != EAGAIN; f = fnext(t.cur[ER_MAIN], f)) {
                assertion("Message length shall be positive", f.pe > f.p);
                const byte_t ack[] = { e ? 0x15 : 0x06 };
                *t.cur[EW_MAIN]++ = ack[0];
                FD_SET((*channel)[EW_MAIN], &t.set[FD_WRITE]);
                if (0 == e) {
                    struct frob_msg parsed = { .magic = FROB_MAGIC };
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
        .t2 = FS "T2" FS "170" FS "TEST" FS "SIM" FS "0" FS ETX
    };
    const time_t timeout = 0;
    if (event_loop(&pm, &channel, timeout) == 0)
        return timeout ? EXIT_FAILURE : EXIT_SUCCESS;
}
