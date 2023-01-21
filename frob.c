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
#include <unistd.h>
#include <fcntl.h>

#ifndef IO_BUF_SIZE
#   define IO_BUF_SIZE (4 * 1024)
#endif

// Don't reorder!
enum channel {
    CHANNEL_NONE = -1,

    CHANNEL_NO_PAYMENT,
    CHANNEL_NO_STORAGE,
    CHANNEL_NO_UI,
    CHANNEL_NO_EVENTS,
    CHANNEL_CO_MASTER,
    CHANNEL_FO_MAIN,

    CHANNEL_II_SIGNAL,
    CHANNEL_CI_MASTER,
    CHANNEL_NI_DEVICE,
    CHANNEL_FI_MAIN,

    CHANNEL_COUNT,

    CHANNEL_FIRST_INPUT  = CHANNEL_II_SIGNAL,
    CHANNEL_FIRST_OUTPUT = CHANNEL_NO_PAYMENT,
    CHANNEL_LAST_INPUT   = CHANNEL_FI_MAIN,
    CHANNEL_LAST_OUTPUT  = CHANNEL_FO_MAIN,
    CHANNEL_FIRST        = CHANNEL_NO_PAYMENT,
    CHANNEL_LAST         = CHANNEL_FI_MAIN
};

// FIXME: Should be removed and constructed from actual values
enum hardcoded_message {
    H_NONE = -1, H_T2, H_T4, H_T5, H_D5, H_B2, H_S2, H_K0, H_K1, H_COUNT
};

struct state {
    const char* hm[H_COUNT];

    struct statistics {
        unsigned short received_good,
                received_bad_lrc,
                received_bad_parse,
                received_bad_handled;
    } stats;

    // Signals to be handled by signalfd
    sigset_t sigfdset;

    struct fstate {
        fd_set eset;
        fd_set wset;
        fd_set rset;
        struct chstate {
            int fd;
            input_t* cur;
            input_t buf[IO_BUF_SIZE];
        } ch[CHANNEL_COUNT];
    } fs;

    struct select_params {
        short maxfd;
        short timeout_sec;
    } select_params;
};

static const char* channel_to_string(const enum channel o) {
    switch (o) {
        case CHANNEL_NO_PAYMENT: return "PAYMENT (native output)";
        case CHANNEL_NO_STORAGE: return "STORAGE (native output)";
        case CHANNEL_NO_UI:      return "UI (native output)";
        case CHANNEL_NO_EVENTS:  return "EVENTS (native output)";
        case CHANNEL_NI_DEVICE:  return "DEVICE (native input)";
        case CHANNEL_FO_MAIN:    return "MAIN (foreign output)";
        case CHANNEL_FI_MAIN:    return "MAIN (foreign input)";
        case CHANNEL_CI_MASTER:  return "MASTER (console input)";
        case CHANNEL_CO_MASTER:  return "MASTER (console output)";
        case CHANNEL_II_SIGNAL:  return "SIGFD (internal input)";
        case CHANNEL_NONE:
        case CHANNEL_COUNT:
            break;
    }
    return NULL;
}

static char channel_to_code(const enum channel o) {
    switch (o) {
        case CHANNEL_NO_PAYMENT: return 'P';
        case CHANNEL_NO_STORAGE: return 'S';
        case CHANNEL_NO_UI:      return 'U';
        case CHANNEL_NO_EVENTS:  return 'E';
        case CHANNEL_NI_DEVICE:  return 'D';
        case CHANNEL_FO_MAIN:
        case CHANNEL_FI_MAIN:    return 'M';
        case CHANNEL_CI_MASTER:
        case CHANNEL_CO_MASTER:  return 'C';
        case CHANNEL_II_SIGNAL:  return 'I';
        case CHANNEL_NONE:
        case CHANNEL_COUNT:
            break;
    }
    return '?';
}

static char class_to_char(const enum FrobMessageType m) {
    switch (m & FROB_MESSAGE_CLASS_MASK) {
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

static const char* frob_message_to_string(const enum FrobMessageType m) {
    static char buf[3];
    buf[0] = class_to_char(m);
    buf[1] = (m & FROB_MESSAGE_NUMBER_MASK) + '0';
    buf[2] = '\0';
    return buf;
}

static int parse_message(const input_t* const p, const input_t* const pe, struct frob_msg* const msg) {
    const input_t* cur = p;
    const char* err;
    int e;
    if ((e = frob_header_extract(&cur, pe, &msg->header)) != 0) {
        err = "Header";
        goto bail;
    }
    if ((e = frob_body_extract(msg->header.type, &cur, pe, &msg->body)) != 0) {
        err = "Body";
        goto bail;
    }
    if ((e = frob_extract_additional_attributes(&cur, pe, &msg->attr)) != 0) {
        err = "Attributes";
        goto bail;
    }
    // Complete message shall be processed, ie cursor shall point to the end of message
    assert(cur == pe);
    return 0;
bail:
    LOGEX("%s parsing failed: %s", err, strerror(e));
    char tmp[4 * (pe - p)];
    LOGDX("\t%s", PRETTV(p, pe, tmp));
    LOGDX("\t%*s", (int)(cur - p), "^");
    return -1;
}

static ssize_t place_message(const size_t s, input_t cur[static const s], const struct frob_msg* const msg) {
    // Magic string should match
    assert(strcmp(msg->magic, FROB_MAGIC) == 0);

    if (sizeof *msg >= s)
        return -ENOBUFS;
    memcpy(cur, msg, sizeof *msg);
    return sizeof *msg;
}

static ssize_t place_frame(const size_t s, input_t cur[static const s], const char (* const token)[6], const char* const body) {
    const int ret = snprintf((char*)cur, s, STX "%s" FS "%s" ETX "_", *token, body);
    if (ret < 0)
        return -errno;
    else if ((unsigned)ret >= s)
        return -ENOBUFS;
    uint8_t lrc = 0;
    for (const input_t* c = cur + 1; c < cur + ret; c++)
        lrc ^= *c;
    cur[ret - 1] = lrc;

    // Parseable frame is created
    assert(frob_frame_process(&(struct frob_frame_fsm_state){.p = cur, .pe = cur + s}) == 0);
    // Parseable message within that frame is created
    assert(parse_message(cur + 1, cur + s - 2, &(struct frob_msg){ .magic = FROB_MAGIC }) == 0);

    return ret;
}

static enum channel choose_destination(const struct frob_msg* const msg) {
    switch (msg->header.type & FROB_MESSAGE_CHANNEL_MASK) {
        case FROB_PAYMENT:
            if (msg->header.type == FROB_S1)
                switch (msg->body.s1.transaction_type) {
                    case FROB_TRANSACTION_TYPE_PAYMENT:
                        return CHANNEL_NO_PAYMENT;
                    case FROB_TRANSACTION_TYPE_VOID:
                        return CHANNEL_NO_STORAGE;
                }
            break;
        case FROB_UI: return CHANNEL_NO_UI;
        case FROB_EVENT: return CHANNEL_NO_EVENTS;
        case FROB_LOCAL: return CHANNEL_FO_MAIN;
        default:
            break;
    }
    // All channels must be handled
    assert(0);
    return CHANNEL_NONE;
}

static enum hardcoded_message choose_hardcoded_response(const enum FrobMessageType t) {
    switch (t) {
        case FROB_T1: return H_T2;
        case FROB_T3: return H_T4;
        case FROB_T4: return H_T5;
        case FROB_D4: return H_D5;
        case FROB_B1: return H_B2;
        case FROB_S1: return H_S2;
        case FROB_K1: return H_K0;
        default:
            break;
    }
    // All messages must be handled
    assert(0);
    return -1;
}

static int commission_message(struct state* const st, const struct frob_msg* const msg) {
    enum channel dst = choose_destination(msg);
    if (dst == CHANNEL_NONE)
        return LOGEX("No destination for message %s: %s", frob_message_to_string(msg->header.type), strerror(ECHRNG)), -1;
    if (st->fs.ch[dst].fd == -1) {
        LOGWX("Channel %s is not connected: %s", channel_to_string(dst), strerror(EUNATCH));
        dst = CHANNEL_FO_MAIN;
    }
    struct chstate* const ch = &st->fs.ch[dst];
    int res;
    if (dst == CHANNEL_FO_MAIN) {
        const enum hardcoded_message h = choose_hardcoded_response(msg->header.type);
        if (h == H_NONE)
            return LOGEX("No hardcoded response for message %s", frob_message_to_string(msg->header.type)), -1;
        res = place_frame(ch->cur - ch->buf, ch->cur, &msg->header.token, st->hm[h]);
    } else {
        res = place_message(ch->cur - ch->buf, ch->cur, msg);
    }
    if (res < 0)
        return LOGEX("Failed to place message %s: %s", frob_message_to_string(msg->header.type), strerror(-res)), -1;
    ch->cur += res;
    FD_SET(ch->fd, &st->fs.wset);
    return 0;
};

static void commission_prompt(struct state* const st) {
    static const char prompt[] = "> ";
    struct chstate* const ch = &st->fs.ch[CHANNEL_CO_MASTER];
    memcpy(ch->cur, prompt, sizeof prompt);
    ch->cur += sizeof prompt;
    FD_SET(ch->fd, &st->fs.wset);
}

static int setup_signalfd(const int ch, const sigset_t blocked) {
    if (sigprocmask(SIG_BLOCK, &blocked, NULL) != 0)
        LOGF("Couldn't adjust signal mask");
    const int ret = signalfd(ch, &blocked, SFD_CLOEXEC);
    if (ret == -1)
        LOGF("Couldn't setup sigfd");
    return ret;
}

static int get_max_fd(const struct chstate (* const ch)[CHANNEL_COUNT]) {
    int max = (*ch)[0].fd;
    for (int i = 1; i < CHANNEL_COUNT; i++)
        max = max < (*ch)[i].fd ? (*ch)[i].fd : max;
    return max + 1;
}

static void start_master_channel(struct state* const s) {
    LOGWX("Master channel doesn't work: %s", strerror(ENOSYS));
    const char* const pts = ttyname(STDERR_FILENO);
    const int fd = open(pts, O_RDWR);
    if (fd == -1)
        return LOGW("Couldn't start %s channel at %s", channel_to_string(CHANNEL_CI_MASTER), pts);
    else
        LOGIX("Master channel started at %s, but what will you do with it?", pts);
    LOGIX("Press ^C again to exit the program or ^D to close master channel (and go back to normal)...");

    s->fs.ch[CHANNEL_CI_MASTER].fd = s->fs.ch[CHANNEL_CO_MASTER].fd = fd;
    sigdelset(&s->sigfdset, SIGINT);
    s->fs.ch[CHANNEL_II_SIGNAL].fd = setup_signalfd(s->fs.ch[CHANNEL_II_SIGNAL].fd, s->sigfdset);
    s->select_params.maxfd = get_max_fd(&s->fs.ch);

    sigset_t tmp;
    sigemptyset(&tmp);
    sigaddset(&tmp, SIGINT);
    if (sigprocmask(SIG_UNBLOCK, &tmp, NULL) != 0)
        LOGF("Can't unblock received singal");

    commission_prompt(s);
}

static struct frob_frame_fsm_state fnext(byte_t* const cursor, const struct frob_frame_fsm_state prev) {
    return (struct frob_frame_fsm_state){
        .p = prev.pe + 2,
        .pe = cursor
    };
}

static void print_stats(const struct statistics* const st) {
    const unsigned int bad = st->received_bad_lrc + st->received_bad_parse + st->received_bad_handled;
    const double ratio = (double)st->received_good / (bad + st->received_good);
    const unsigned total = st->received_good + bad;
    LOGIX("Current stats:");
    LOGIX("	Received messages");
    LOGIX("	Total	Ratio	Good	LRC	parse	handling");
    LOGIX("	%u	%.5f	%u	%u	%u	%u", total, ratio, st->received_good,
            st->received_bad_lrc, st->received_bad_parse, st->received_bad_handled);
}

static void process_signal(struct state* const s) {
    // FIXME: Multiple signals can be placed into the input buffer
    assert(sizeof (struct signalfd_siginfo) == s->fs.ch[CHANNEL_II_SIGNAL].cur - s->fs.ch[CHANNEL_II_SIGNAL].buf);
    const struct signalfd_siginfo* const inf = (struct signalfd_siginfo*)s->fs.ch[CHANNEL_II_SIGNAL].buf;
    switch (inf->ssi_signo) {
        case SIGINT:
            start_master_channel(s);
            break;
        case SIGALRM:
            // TODO: Reschedule last message, but only if underlying fd isn't already closed
            LOGWX("Can't re-transmit: %s", strerror(ENOSYS));
            break;
        case SIGPWR:
            print_stats(&s->stats);
            break;
        default:
            LOGFX("Unexpected signal. Bailing out");
    }
    s->fs.ch[CHANNEL_II_SIGNAL].cur = s->fs.ch[CHANNEL_II_SIGNAL].buf;
}

static void process_master(struct state* const s) {
    LOGWX("No commands are currently supported: %s", strerror(ENOSYS));
    commission_prompt(s);
}

static void process_main(struct state* const s, struct frob_frame_fsm_state* const f) {
    // Disarm any pending re-transmission alarm
    alarm(0);
    int e;
    for (f->pe = s->fs.ch[CHANNEL_FI_MAIN].cur; (e = frob_frame_process(f)) != EAGAIN; *f = fnext(s->fs.ch[CHANNEL_FI_MAIN].cur, *f)) {
        // Message length shall be positive
        assert(f->pe > f->p);
        const byte_t ack[] = { e ? 0x15 : 0x06 };
        *s->fs.ch[CHANNEL_FO_MAIN].cur++ = ack[0];
        FD_SET(s->fs.ch[CHANNEL_FO_MAIN].fd, &s->fs.wset);
        if (0 == e) {
            struct frob_msg parsed = { .magic = FROB_MAGIC };
            if (parse_message(f->p, f->pe, &parsed) != 0) {
                LOGWX("Can't process message");
                s->stats.received_bad_parse++;
            } else {
                if (commission_message(s, &parsed) != 0) {
                    LOGWX("Message wasn't handled");
                    s->stats.received_bad_handled++;
                } else {
                    s->stats.received_good++;
                }
            }
        } else {
            LOGWX("Can't parse incoming frame: %s", strerror(e));
            s->stats.received_bad_lrc++;
        }
    }
}

static void process_channel(const enum channel c, struct state* const s, struct frob_frame_fsm_state* const r) {
    switch (c) {
        case CHANNEL_II_SIGNAL: return process_signal(s);
        case CHANNEL_CI_MASTER: return process_master(s);
        case CHANNEL_FI_MAIN:   return process_main(s, r);
        case CHANNEL_NI_DEVICE:
            LOGWX("Device channel isn't supported yet");
            break;
        default:
            assert(false);
            break;
    }
}

static void perform_pending_write(struct chstate* const ch) {
    // FIXME: select works on file descriptors and not on channels, so when same fd is assigned to
    //        different channels then the first channel (which may be empty) will be used and then
    //        the whole fd will be cleared causing data loss on the other channel
    if (ch->cur == ch->buf)
        return;
    // We shouldn't attempt to write 0 bytes
    assert(ch->cur > ch->buf);
    // FIXME: Retry if we were unable to write all bytes
    const ssize_t s = write(ch->fd, ch->buf, ch->cur - ch->buf);
    if (s != ch->cur - ch->buf) {
        //LOGF("Can't write %td bytes to %s channel (fd %d)", t->cur[i] - t->buf[i], channel_to_string(i), (*channel)[i]);
    } else {
        // Not just ACK/NAK alone – very bad heuristic
        if (ch->cur - ch->buf > 1) {
            const unsigned int prev = alarm(3);
            // FIXME: Enforce that only single message can be sent at a time until ACK/NAK wasn't received
            if (prev != 0)
                LOGWX("Duplicated alram scheduled. Previous was ought to be delivered in %us", prev);
            //assertion("Only singal active timeout is expected", prev == 0);
        }
        //char tmp[3*s];
        //LOGDX("← %c %zu\t%s", channel_to_code(i), s, PRETTV(t->buf[i], t->cur[i], tmp));
    }
    // FIXME: This pointer shall be reset only on ACK or last retransmission
    ch->cur = ch->buf;
}

static void perform_pending_read(struct chstate* const ch) {
    const ssize_t s = read(ch->fd, ch->cur, ch->buf + sizeof ch->buf - ch->cur);
    if (s < 0) {
        //LOGF("Can't read data on %s channel (fd %d)", channel_to_string(i), (*channel)[i]);
    } else if (s == 0) {
        /*
        LOGIX("Channel %s (fd %d) was closed", channel_to_string(i), (*channel)[i]);
        close((*channel)[i]);
        FD_CLR((*channel)[i], &(t->set)[FDSET_READ]);
        switch (i) {
            case CHANNEL_FI_MAIN:
                // TODO: Set timer to some small value or in some other wise schedule program to exit in a short time
                break;
            case CHANNEL_CI_MASTER:
                (*channel)[CHANNEL_II_SIGNAL] = setup_signalfd((*channel)[CHANNEL_II_SIGNAL], blocked);
                break;
        }
        (*channel)[i] = -1;
        */
    } else {
        //char tmp[3*s];
        //LOGDX("→ %c %zu\t%s", channel_to_code(i), s, PRETTV(t->cur[i], t->cur[i] + s, tmp));
    }
    ch->cur += s;
}

static void perform_pending_io(struct fstate* const f) {
    for (enum channel i = CHANNEL_FIRST; i <= CHANNEL_LAST; i++) {
        if (FD_ISSET(f->ch[i].fd, &f->eset))
            LOGFX("Exceptional data isn't supported");
        if (FD_ISSET(f->ch[i].fd, &f->wset))
            perform_pending_write(&f->ch[i]);
        if (FD_ISSET(f->ch[i].fd, &f->rset))
            perform_pending_read(&f->ch[i]);
    }
    for (enum channel i = CHANNEL_FIRST; i <= CHANNEL_LAST; i++)
        FD_CLR(f->ch[i].fd, &f->wset);
}

static void fset(struct fstate* const f) {
    for (enum channel i = CHANNEL_NO_PAYMENT; i < CHANNEL_COUNT; i++) {
        const int fd = f->ch[i].fd;
        if (fd == -1 || fd >= FD_SETSIZE)
            continue;
        FD_SET(fd, &f->eset);
        switch (i) {
            case CHANNEL_NI_DEVICE:
            case CHANNEL_FI_MAIN:
            case CHANNEL_CI_MASTER:
            case CHANNEL_II_SIGNAL:
                FD_SET(fd, &f->rset);
            default:
                break;
        }
    }
}

static void finit(struct fstate* const f) {
    FD_ZERO(&f->eset);
    FD_ZERO(&f->wset);
    FD_ZERO(&f->eset);
    for (enum channel i = CHANNEL_NO_PAYMENT; i < CHANNEL_COUNT; i++)
        f->ch[i].cur = f->ch[i].buf;
    fset(f);
}

static int fselect(struct fstate* const f, const struct select_params* const s) {
    struct timeval t = { .tv_sec = s->timeout_sec };
    const int l = select(s->maxfd, &f->rset, &f->wset, &f->eset, (s->timeout_sec < 0 ? NULL : &t));
    if (l == 0)
        if (s->timeout_sec == 0)
            LOGIX("Single-shot mode ended");
        else
            LOGWX("Timed out");
    else if (l < 0)
        LOGE("Select failed");
    return l;
}

static int event_loop(struct state* const s) {
    struct frob_frame_fsm_state r = { .p = s->fs.ch[CHANNEL_FI_MAIN].buf };
    int ret = 0;
    for (finit(&s->fs); (ret = fselect(&s->fs, &s->select_params)) > 0; fset(&s->fs)) {
        perform_pending_io(&s->fs);
        for (enum channel i = CHANNEL_FIRST_INPUT; i <= CHANNEL_LAST_INPUT; i++) {
            if (s->fs.ch[i].fd >= 0 && FD_ISSET(s->fs.ch[i].fd, &s->fs.rset))
                process_channel(i, s, &r);
        }
    }
    // Event loop shall end only on error/timeout/interrupt
    assert(ret <= 0);
    return ret;
}

static sigset_t adjust_signal_delivery(int* const ch) {
    static const int blocked_signals[] = {
        SIGALRM,
        SIGPWR,
        SIGINT
    };
    sigset_t s;
    sigemptyset(&s);
    for (size_t i = 0; i < elementsof(blocked_signals); i++)
        sigaddset(&s, blocked_signals[i]);
    *ch = setup_signalfd(*ch, s);
    return s;
}

int main(const int ac, const char* av[static const ac]) {
    static struct state s = {
        .hm = {
            [H_T2] = FS "T2" FS "170" FS "TEST" FS "SIM" FS "0" FS,
            [H_B2] = FS "B2" FS "170" FS "TEST" FS "SIM" FS "0" FS "0" FS "0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000" FS "00" FS,
            [H_T4] = FS "T4" FS "160" US "170" US FS,
            [H_T5] = FS "T5" FS "170" FS,
            [H_S2] = FS "S2" FS "993" FS FS "M000" FS "T000" FS "N/A" FS FS FS "NONE" FS "Payment endopoint not available" FS,
            [H_K0] = FS "K0" FS "0" FS,
            [H_D5] = FS "D5" FS "24" FS "12" FS "6" FS "19" FS "1" FS "1" FS "1"
                    FS "0" FS "0" FS "0" FS FS FS "4" FS "9999" FS "4" FS "15"
                    FS "ENTER" US "CANCEL" US "CHECK" US "BACKSPACE" US "DELETE" US "UP" US "DOWN" US "LEFT" US "RIGHT" US
                    FS "1" FS "1" FS "1" FS "0" FS
        }
    };
    s.sigfdset = adjust_signal_delivery(&s.fs.ch[CHANNEL_II_SIGNAL].fd);
    s.fs.ch[CHANNEL_FO_MAIN].fd = STDOUT_FILENO;
    s.fs.ch[CHANNEL_FI_MAIN].fd = STDIN_FILENO;
    //s.channel[CHANNEL_NO_PAYMENT] = open("payment", O_WRONLY | O_CLOEXEC);
    s.select_params = (struct select_params) {
        .timeout_sec = ac == 2 ? atoi(av[1]) : 0,
        .maxfd = get_max_fd(&s.fs.ch)
    };

    if (event_loop(&s) == 0)
        return s.select_params.timeout_sec > 0 ? EXIT_FAILURE : EXIT_SUCCESS;
    return EXIT_FAILURE;
}
