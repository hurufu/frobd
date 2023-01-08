#include "frob.h"
#include "utils.h"
#include "log.h"

#include <stdlib.h>
#include <signal.h>
#include <sys/signalfd.h>
#include <stdio.h>
#include <errno.h>
#include <assert.h>
#include <poll.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

// FIXME: Reduce number of arguments passed to functions

#define IO_BUF_SIZE (4 * 1024)

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
    MR_SIGNAL,

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
    byte_t buf[CHANNELS_COUNT][IO_BUF_SIZE];
};

struct preformated_messages {
    byte_t t2[32];
    byte_t t4[32];
    byte_t t5[32];
    byte_t d5[128];
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
        case MR_SIGNAL:  return "SIGFD";
        case CHANNELS_COUNT:
            break;
    }
    return NULL;
}

static char frob_message_class_to_char(const enum FrobMessageType m) {
    switch (m & 0xF0) {
        case FROB_T: return 'T';
        case FROB_D: return 'D';
        case FROB_S: return 'S';
        case FROB_P: return 'P';
        case FROB_I: return 'I';
        case FROB_A: return 'A';
        case FROB_K: return 'K';
        case FROB_M: return 'M';
        case FROB_L: return 'L';
        case FROB_B: return 'B';
    }
    return '?';
}

static const char* frob_message_destination_to_string(const enum FrobMessageType m) {
    switch (m & FROB_DESTINATION_MASK) {
        case FROB_LOCAL: return "Local";
        case FROB_PAYMENT: return "Payment";
        case FROB_STORAGE: return "Storage";
        case FROB_MAPPED: return "Mapped";
        case FROB_UI: return "UI";
        case FROB_EVENT: return "Event";
    }
    return NULL;
}

static const char* frob_message_to_string(const enum FrobMessageType m) {
    static char buf[3];
    buf[0] = frob_message_class_to_char(m);
    buf[1] = (m & 0x0F) + '0';
    buf[2] = '\0';
    return buf;
}

static char channel_to_code(const enum OrderedChannels o) {
    switch (o) {
        case IW_PAYMENT: return 'P';
        case IW_STORAGE: return 'S';
        case IW_UI:      return 'U';
        case IW_EVENTS:  return 'E';
        case IR_DEVICE:  return 'D';
        case EW_MAIN:    return 'M';
        case ER_MAIN:    return 'M';
        case ER_MASTER:  return 'R';
        case MR_SIGNAL:  return 'I';
        case CHANNELS_COUNT:
            break;
    }
    return '-';
}

static int forward_message(const struct frob_msg* const msg, const enum OrderedChannels channel, const int fd, struct io_state* const t) {
    assert(channel >= 0 && channel < CHANNELS_COUNT);
    if (t->cur[channel] + sizeof msg >= t->buf[channel] + sizeof t->buf[channel])
        return LOGWX("Message forwarding skipped: %s", strerror(ENOBUFS)), -1;
    if (fd < 0 || fd >= FD_SETSIZE)
        return LOGWX("Message forwarding skipped for fd %d: %s", fd, strerror(EBADF)), -1;
    memcpy(t->cur[channel], msg, sizeof *msg);
    t->cur[channel] += sizeof *msg;
    FD_SET(fd, &t->set[FD_WRITE]);
    return 0;
}

static int process_msg(const unsigned char* p, const unsigned char* const pe, struct frob_msg* const msg) {
    msg->header = frob_header_extract(&p, pe);
    if (msg->header.type == 0)
        return LOGWX("Bad header"), -1;

    int e;
    switch (e = frob_body_extract(msg->header.type, &p, pe, &msg->body)) {
        case 0:
            break;
        case EBADMSG:
        case ENOSYS:
        case ENOTRECOVERABLE:
            return LOGEX("Body not parsed: %s", strerror(e)), -1;
        default:
            assert(false);
    }

    switch (e = frob_extract_additional_attributes(&p, pe, &msg->attr)) {
        case 0:
            break;
        case EBADMSG:
        case ENOTRECOVERABLE:
            return LOGEX("Attrs not parsed: %s", strerror(e)), -1;
        default:
            assert(false);

    }

    assertion("Complete message shall be processed, ie cursor shall point to the end of message", p == pe);
    return 0;
}

static int make_frame(const byte_t* const body, const byte_t (* const token)[3], byte_t** const pp, byte_t* const pe) {
    byte_t* p = *pp;

    assert(pe >= p && pe - p <= IO_BUF_SIZE);

    const size_t l = strlen((char*)body);
    if (pe - *pp < 1 + 6 + (int)l + 1) // STX + token + body + LRC
        return LOGWX("Can't construct frame: %s", strerror(ENOBUFS)), -1;

    const int r = snprintf((char*)p, pe - p, "\x02%02X%02X%02X", (*token)[0], (*token)[1], (*token)[2]);
    if (r != 7)
        return LOGW("Can't construct token"), -1;
    p += r;
    memcpy(p, body, l);
    p += l;
    uint8_t lrc = 0;
    for (const byte_t* c = *pp + 1; c <= p; c++)
        lrc ^= *c;
    *p++ = lrc;

    assert(pe >= p && pe - p <= IO_BUF_SIZE);
    assertion("Parseable frame is created", frob_frame_process(&(struct frob_frame_fsm_state){.p = *pp, .pe = p}) == 0);
    assertion("Parseable message within is created", process_msg(*pp + 1, p - 2, &(struct frob_msg){ .magic = FROB_MAGIC }) == 0);

    *pp = p;
    return 0;
}

static int handle_local(const struct preformated_messages* const pm, const struct frob_header* const h, int (*channel)[CHANNELS_COUNT], struct io_state* const t, int* const expected_acks) {
    const byte_t* m = NULL;
    switch (h->type) {
        case FROB_T1: m = pm->t2; break;
        case FROB_T3: m = pm->t4; break;
        case FROB_T4: m = pm->t5; break;
        case FROB_D4: m = pm->d5; break;
        case FROB_T2:
        case FROB_T5:
        case FROB_D5:
            LOGIX("%s message %s (%#x) skipped", frob_message_destination_to_string(h->type), frob_message_to_string(h->type), h->type);
            return 0;
        case FROB_S1:
        case FROB_S2:
        case FROB_P1:
        case FROB_I1:
        case FROB_A1:
        case FROB_A2:
            LOGFX("Internal error, message %d shouldn't be handled locally", h->type);
            return -1;
        default:
            LOGWX("There isn't any answer to reply: %s", strerror(ENOSYS));
            return -1;
    }
    if (make_frame(m, &h->token, &t->cur[EW_MAIN], endof(t->buf[EW_MAIN])) != 0)
        return LOGWX("Locally generate response skipped"), -1;
    FD_SET((*channel)[EW_MAIN], &t->set[FD_WRITE]);
    (*expected_acks)++;
    return 0;
}

static int get_destination_channel(const struct frob_msg* const msg) {
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

static int handle_message(const struct preformated_messages* const pm, const struct frob_msg* const msg, int (*channel)[CHANNELS_COUNT], struct io_state* const t, int* const expected_acks) {
    assertion("Magic string shall match", strcmp(msg->magic, FROB_MAGIC) == 0);
    const int ch = get_destination_channel(msg);
    if (ch == -2)
        return -2;
    if (ch == -1)
        return handle_local(pm, &msg->header, channel, t, expected_acks);
    return forward_message(msg, ch, (*channel)[ch], t);
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
                case MR_SIGNAL:
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

static int fselect(const int nfd, fd_set (* const set)[FD_SET_COUNT], const time_t relative_timeout) {
    struct timeval t = { .tv_sec = relative_timeout };
    struct timeval* const tp = (relative_timeout == (time_t)-1) ? NULL : &t;
    const int l = select(nfd, &(*set)[FD_READ], &(*set)[FD_WRITE], &(*set)[FD_EXCEPT], tp);
    if (l == 0)
        if (relative_timeout)
            LOGWX("%s", strerror(ETIME));
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

static void process_signal(sigset_t blocked, struct io_state* const t, int (*channel)[CHANNELS_COUNT]) {
    // TODO: Use this fd to enable master channel on SIGINT
    // TODO: Use this fd to Handle missing ACK/NAK using SIGALRM
    assert(sizeof (struct signalfd_siginfo) == t->cur[MR_SIGNAL] - t->buf[MR_SIGNAL]);
    const struct signalfd_siginfo* const inf = (struct signalfd_siginfo*)t->buf[MR_SIGNAL];
    sigdelset(&blocked, inf->ssi_signo);
    (*channel)[MR_SIGNAL] = signalfd((*channel)[MR_SIGNAL], &blocked, SFD_CLOEXEC);
    sigset_t s;
    sigemptyset(&s);
    sigaddset(&s, inf->ssi_signo);
    t->cur[MR_SIGNAL] = t->buf[MR_SIGNAL];
    if (sigprocmask(SIG_UNBLOCK, &s, NULL) != 0)
        LOGF("Can't unblock received singal");
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
                        // FIXME: select works on file descriptors and not on channels, so when same fd is assigned to
                        //        different channels then the first channel (which may be empty) will be used and then
                        //        the whole fd will be cleared causing data loss on the other channel
                        if (t->cur[i] == t->buf[i])
                            continue;
                        assertion("We shouldn't attempt to write 0 bytes", t->cur[i] > t->buf[i]);
                        // FIXME: Retry if we were unable to write all bytes
                        if ((s = write((*channel)[i], t->buf[i], t->cur[i] - t->buf[i])) != t->cur[i] - t->buf[i]) {
                            LOGF("Can't write %td bytes to %s channel (fd %d)", t->cur[i] - t->buf[i], channel_to_string(i), (*channel)[i]);
                        } else {
                            char tmp[3*s];
                            LOGDX("← %c %zu\t%s", channel_to_code(i), s, PRETTV(t->buf[i], t->cur[i], tmp));
                        }
                        t->cur[i] = t->buf[i];
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
                            LOGDX("→ %c %zu\t%s", channel_to_code(i), s, PRETTV(t->cur[i], t->cur[i] + s, tmp));
                        }
                        t->cur[i] += s;
                        break;
                }
            }
        }
    }
    for (size_t i = 0; i < elementsof(*channel); i++)
        FD_CLR((*channel)[i], &(t->set)[FD_WRITE]);
}

static struct frob_frame_fsm_state fnext(byte_t* const cursor, const struct frob_frame_fsm_state prev) {
    return (struct frob_frame_fsm_state){
        .p = prev.pe + 2,
        .pe = cursor
    };
}

static int event_loop(const struct preformated_messages* const pm, int (* const channel)[CHANNELS_COUNT], const time_t relative_timeout, const sigset_t blocked) {
    static struct io_state t;

    const int m = get_max_fd(channel);
    struct frob_frame_fsm_state f = { .p = t.buf[ER_MAIN] };
    int expected_acks = 0;
    int ret = 0;
    for (finit(channel, &t); (ret = fselect(m, &t.set, relative_timeout)) > 0; fset(channel, &t.set)) {
        perform_pending_io(&t, channel);

        if (FD_ISSET((*channel)[MR_SIGNAL], &t.set[FD_READ]))
            process_signal(blocked, &t, channel);

        if (FD_ISSET((*channel)[ER_MAIN], &t.set[FD_READ])) {
            // FIXME: This is a wrong way of checking ACK/NAK
            for (int i = 0; i < expected_acks; i++)
                switch (t.cur[ER_MAIN][i]) {
                    case 0x00: // Official ECR-EFT simulator sends null byte at the end
                    case 0x06: // Positive acknowledge
                       break;
                    case 0x15:
                        // TODO: Retransmit message when needed
                        LOGWX("Can't retransmit: %s", strerror(ENOSYS));
                        break;
                    default:
                        LOGF("Unexpected character [%02X] in stream, bailing out", t.cur[ER_MAIN][i]);
                        break;
                }
            expected_acks = 0;
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
                        if (handle_message(pm, &parsed, channel, &t, &expected_acks) != 0)
                            LOGWX("Message wasn't handled");
                } else {
                    LOGWX("Can't parse incoming frame: %s", strerror(e));
                }
            }
        }
    }

    assert(ret <= 0);
    return ret;
}

static sigset_t adjust_signal_delivery(int* const ch) {
    static const int blocked_signals[] = {
        SIGALRM,
        SIGINT
    };
    sigset_t s;
    sigemptyset(&s);
    for (size_t i = 0; i < elementsof(blocked_signals); i++)
        sigaddset(&s, blocked_signals[i]);
    if (sigprocmask(SIG_BLOCK, &s, NULL) != 0)
        LOGF("Couldn't adjust signal mask");
    *ch = signalfd(-1, &s, SFD_CLOEXEC);
    if (*ch == -1)
        LOGF("Couldn't setup sigfd");
    return s;
}

int main(const int ac, const char* av[static const ac]) {
    static int channel[] = {
        [IW_PAYMENT] = STDOUT_FILENO,
        [IW_STORAGE] = STDOUT_FILENO,
        [IW_UI     ] = STDOUT_FILENO,
        [IW_EVENTS ] = STDOUT_FILENO,
        [IR_DEVICE ] = -1,
        [EW_MAIN   ] = STDOUT_FILENO,
        [ER_MAIN   ] = STDIN_FILENO,
        [ER_MASTER ] = -1,
        [MR_SIGNAL ] = -1
    };

    const sigset_t blocked = adjust_signal_delivery(&channel[MR_SIGNAL]);
    if (!all_channels_ok(&channel))
        return EXIT_FAILURE;

    static const struct preformated_messages pm = {
        .t2 = FS "T2" FS "170" FS "TEST" FS "SIM" FS "0" FS ETX,
        .t4 = FS "T4" FS "160" US "170" US FS ETX,
        .t5 = FS "T5" FS "170" FS ETX,
        .d5 = FS "D5" FS "24" FS "12" FS "6" FS "19" FS "1" FS "1" FS "1"
              FS "0" FS "0" FS "0" FS FS FS "4" FS "9999" FS "4" FS "15"
              FS "ENTER" US "CANCEL" US "CHECK" US "BACKSPACE" US "DELETE" US "UP" US "DOWN" US "LEFT" US "RIGHT" US
              FS "1" FS "1" FS "1" FS "0" FS ETX
    };

    const time_t timeout = ac == 2 ? atoi(av[1]) : 0;
    if (event_loop(&pm, &channel, timeout, blocked) == 0)
        return timeout ? EXIT_FAILURE : EXIT_SUCCESS;
    return EXIT_FAILURE;
}
