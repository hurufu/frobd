#include "frob.h"
#include "utils.h"
#include "log.h"

#include <signal.h>
#include <sys/signalfd.h>
#include <stdio.h>
#include <errno.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/resource.h>

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

enum role {
    ROLE_ECR,
    ROLE_EFT
};

struct state {
    const char* hm[H_COUNT];
    const enum role role;

    struct statistics {
        unsigned short received_good,
                received_bad_lrc,
                received_bad_parse,
                received_bad_handled;
    } stats;

    // Signals to be handled by signalfd
    sigset_t sigfdset;

    int ack;

    unsigned short pings_on_inactivity_left;

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

static enum channel choose_destination_from_message(const struct frob_msg* const msg) {
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

static enum channel choose_destination(const struct fstate* const f, const struct frob_msg* const msg) {
    enum channel dst = choose_destination_from_message(msg);
    if (dst == CHANNEL_NONE)
        return LOGEX("No destination for message %s: %s", frob_message_to_string(msg->header.type), strerror(ECHRNG)), -1;
    if (f->ch[dst].fd == -1) {
        LOGWX("Channel %s is not connected: %s", channel_to_string(dst), strerror(EUNATCH));
        dst = CHANNEL_FO_MAIN;
    }
    return dst;
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
        case FROB_T2:
        case FROB_T5: return H_NONE;
        default:
            LOGWX("No hardcoded response for %s (%#x)", frob_message_to_string(t), t);
            break;
    }
    // All messages must be handled
    assert(false);
    return -1;
}

static size_t free_space(const struct chstate* const ch) {
    assert(ch->cur >= ch->buf);
    assert(ch->cur <= lastof(ch->buf));
    return lastof(ch->buf) - ch->cur;
}

static void compute_next_token(const enum role r, char (*t)[6]) {
    static unsigned int token = 0;
    const unsigned int offset = r == ROLE_ECR ? 10000 : 20000;
    snprintfx(*t, sizeof *t, "%X", token + offset);

    // Specification doesn't define what to do in case of token overflow
    token = (token + 1) % offset;
}

static void commission_native_message(struct fstate* const f, const enum channel dst, const struct frob_msg* const msg) {
    // Magic string should match
    assert(strcmp(msg->magic, FROB_MAGIC) == 0);

    struct chstate* const ch = &f->ch[dst];
    if (sizeof *msg >= free_space(ch))
        return LOGEX("Message skipped: %s", strerror(ENOBUFS));
    memcpy(ch->cur, msg, sizeof *msg);

    FD_SET(ch->fd, &f->wset);
    ch->cur += sizeof *msg;
}

static void commission_frame_on_main(struct state* const st, const char (* const token)[6], const char* const body) {
    struct fstate* const f = &st->fs;
    struct chstate* const ch = &f->ch[CHANNEL_FO_MAIN];
    const size_t s = free_space(ch);
    const int ret = snprintf((char*)ch->cur, s, STX "%.6s" FS "%s" ETX "_", *token, body);
    if (ret < 0)
        LOGF("Can't place frame");
    if ((unsigned)ret >= s || ret == 0)
        return LOGEX("Frame skipped: %s", (ret ? strerror(ENOBUFS): "Empty message"));
    uint8_t lrc = 0;
    for (const input_t* c = ch->cur + 1; c < ch->cur + ret - 1; c++)
        lrc ^= *c;
    ch->cur[ret - 1] = lrc;

    st->ack = 1;
    FD_SET(ch->fd, &f->wset);

    // Parseable frame is created
    assert(frob_frame_process(&(struct frob_frame_fsm_state){.p = ch->cur, .pe = ch->cur + s}) == 0);
    // Parseable message within that frame is created
    assert(parse_message(ch->cur + 1, ch->cur + ret - 2, &(struct frob_msg){ .magic = FROB_MAGIC }) == 0);

    ch->cur += ret;
}

static int commission_response(struct state* const s, const struct frob_msg* const received_msg) {
    const enum channel dst = choose_destination(&s->fs, received_msg);
    if (dst == CHANNEL_FO_MAIN) {
        if (received_msg->header.type == FROB_T2)
            s->pings_on_inactivity_left++;
        const enum hardcoded_message h = choose_hardcoded_response(received_msg->header.type);
        if (h == H_NONE)
            return LOGDX("Message %s concludes communication sequence", frob_message_to_string(received_msg->header.type)), 0;
        // Reply with hardcoded response
        commission_frame_on_main(s, &received_msg->header.token, s->hm[h]);
    } else {
        // Forward message without changing it
        commission_native_message(&s->fs, dst, received_msg);
    }

    return 0;
};

static void commission_prompt_on_master(struct state* const st) {
    MCOPY(st->fs.ch[CHANNEL_CO_MASTER].cur, "> ");
    FD_SET(st->fs.ch[CHANNEL_CO_MASTER].fd, &st->fs.wset);
}

static int commission_ping_on_main(struct state* const s) {
    if (s->fs.ch[CHANNEL_FO_MAIN].fd < 0)
        return 0;
    char token[6];
    compute_next_token(s->role, &token);
    commission_frame_on_main(s, &token, "T1" FS);
    return s->pings_on_inactivity_left--;
}

static void commission_ack_on_main(struct state* const s, const bool is_nak) {
    *s->fs.ch[CHANNEL_FO_MAIN].cur++ = is_nak ? NAK : ACK ;
    FD_SET(s->fs.ch[CHANNEL_FO_MAIN].fd, &s->fs.wset);
}

static int setup_signalfd(const int ch, const sigset_t blocked) {
    if (sigprocmask(SIG_BLOCK, &blocked, NULL) != 0)
        LOGF("Couldn't adjust signal mask");
    const int fd = signalfd(ch, &blocked, SFD_CLOEXEC);
    if (fd == -1)
        LOGF("Couldn't setup sigfd for %d", ch);
    return fd;
}

// IMPORTANT: Remember to call this function after every fd update
static int get_max_fd(const struct chstate (* const ch)[CHANNEL_COUNT]) {
    int max = (*ch)[0].fd;
    for (int i = 1; i < CHANNEL_COUNT; i++)
        max = max < (*ch)[i].fd ? (*ch)[i].fd : max;
    return max + 1;
}

static void start_master_channel(struct state* const s) {
    LOGWX("Master channel doesn't work: %s", strerror(ENOSYS));
    // TODO: I don't really know what is a good way to implement interactive console.
    //       It should be compatible with rlwrap (1) and it shouldn't interfere
    //       with s6-tcpserver4, because both use stdin/stdout.
    const char* pts = ttyname(STDIN_FILENO);
    if (!pts) {
        LOGW("stdin isn't a tty");
        pts = ctermid(NULL);
        const int fd = open(pts, O_RDWR);
        s->fs.ch[CHANNEL_CI_MASTER].fd = s->fs.ch[CHANNEL_CO_MASTER].fd = fd;
    } else {
        s->fs.ch[CHANNEL_CI_MASTER].fd = STDIN_FILENO;
        s->fs.ch[CHANNEL_CO_MASTER].fd = STDOUT_FILENO;
    }
    LOGIX("Master channel started at %s, but what will you do with it?", pts);

    sigdelset(&s->sigfdset, SIGINT);
    int* const sigfd = &s->fs.ch[CHANNEL_II_SIGNAL].fd;
    *sigfd = setup_signalfd(*sigfd, s->sigfdset);
    s->select_params.maxfd = get_max_fd(&s->fs.ch);

    sigset_t tmp;
    sigemptyset(&tmp);
    sigaddset(&tmp, SIGINT);
    if (sigprocmask(SIG_UNBLOCK, &tmp, NULL) != 0)
        LOGF("Can't unblock received singal");

    MCOPY(s->fs.ch[CHANNEL_CO_MASTER].cur, "Press ^C again to exit the program or ^D end interactive session...\n");
    commission_prompt_on_master(s);
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
    LOGIX("\tReceived messages");
    LOGIX("\tTotal"  "\tRatio"  "\tGood"  "\tLRC"  "\tparse"  "\thandling");
    LOGIX("\t%u"     "\t%.5f"   "\t%u"    "\t%u"   "\t%u"     "\t%u",
            total, ratio, st->received_good,
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
            LOGDX("Retransmission isn't implemented yet");
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
    commission_prompt_on_master(s);
}

static void process_main(struct state* const s, struct frob_frame_fsm_state* const f) {
    int e;
    for (f->pe = s->fs.ch[CHANNEL_FI_MAIN].cur; (e = frob_frame_process(f)) != EAGAIN; *f = fnext(s->fs.ch[CHANNEL_FI_MAIN].cur, *f)) {
        // Message length shall be positive
        assert(f->pe > f->p);
        commission_ack_on_main(s, e);
        if (0 == e) {
            struct frob_msg parsed = { .magic = FROB_MAGIC };
            if (parse_message(f->p, f->pe, &parsed) != 0) {
                LOGWX("Can't process message");
                s->stats.received_bad_parse++;
            } else {
                if (commission_response(s, &parsed) != 0) {
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

static void perform_pending_write(const enum channel i, struct chstate* const ch, const int ack) {
    // We shouldn't attempt to write 0 bytes
    assert(ch->cur > ch->buf);
    // We can write only to output channels
    switch (i) {
        case CHANNEL_FIRST_OUTPUT ... CHANNEL_LAST_OUTPUT:
            break;
        default:
            assert(false);
    }
    const ptrdiff_t l = ch->cur - ch->buf;
    const ssize_t s = write(ch->fd, ch->buf, l);
    if (s != l) {
        LOGF("Can't write %td bytes to %s channel (fd %d)", l, channel_to_string(i), ch->fd);
    } else {
        if (ack == 1 && i == CHANNEL_FO_MAIN) {
            const unsigned int prev = alarm(3);
            if (prev != 0)
                LOGWX("Duplicated alram scheduled. Previous was ought to be delivered in %us", prev);
            // Only one active timeout is expected
            assert(prev == 0);
        }
        char tmp[3*s];
        LOGDX("← %c %zu\t%s", channel_to_code(i), s, PRETTV(ch->buf, ch->cur, tmp));
    }
    // FIXME: This pointer shall be reset only on ACK or last retransmission
    ch->cur = ch->buf;
}

static void perform_pending_read(const enum channel i, struct state* const s) {
    struct chstate* const ch = &s->fs.ch[i];
    const ssize_t r = read(ch->fd, ch->cur, free_space(ch));
    if (r < 0) {
        LOGF("Can't read data on %s channel (fd %d)", channel_to_string(i), ch->fd);
    } else if (r == 0) {
        LOGIX("Channel %s (fd %d) was closed", channel_to_string(i), ch->fd);
        close(ch->fd);
        FD_CLR(ch->fd, &s->fs.rset);
        ch->fd = -1;
        switch (i) {
            case CHANNEL_FI_MAIN:
                // If main channel is closed by remote side then the only
                // meaningful thing we can do is to exit gracefully after all
                // pending writes are done by setting timeout to a small value.
                if (s->select_params.timeout_sec != 0)
                    s->select_params.timeout_sec = 1;
                close(s->fs.ch[CHANNEL_FO_MAIN].fd);
                s->fs.ch[CHANNEL_FO_MAIN].fd = -1;
                break;
            case CHANNEL_CI_MASTER:
                // If console output channel is closed then we should close
                // corresponding input channel as well
                close(s->fs.ch[CHANNEL_CO_MASTER].fd);
                s->fs.ch[CHANNEL_CO_MASTER].fd = -1;
                break;
            case CHANNEL_II_SIGNAL:
            case CHANNEL_NI_DEVICE:
                break;
            default:
                // We can read only from input channels
                assert(false);
        }
        s->select_params.maxfd = get_max_fd(&s->fs.ch);
    } else {
        if (s->ack) {
            switch (ch->cur[0]) {
                case ACK:
                case NAK:
                    alarm(0);
                    s->ack = false;
                    break;
                default:
                    break;
            }
        }
        char tmp[3*r];
        LOGDX("→ %c %zu\t%s", channel_to_code(i), r, PRETTV(ch->cur, ch->cur + r, tmp));
    }
    ch->cur += r;
}

static void perform_pending_io(struct state* const s) {
    for (enum channel i = CHANNEL_FIRST; i <= CHANNEL_LAST; i++) {
        const int fd = s->fs.ch[i].fd;
        if (fd < 0)
            continue;
        if (FD_ISSET(fd, &s->fs.eset))
            LOGFX("Exceptional data isn't supported");
        if (FD_ISSET(fd, &s->fs.wset))
            perform_pending_write(i, &s->fs.ch[i], s->ack);
        if (FD_ISSET(fd, &s->fs.rset))
            perform_pending_read(i, s);
        FD_CLR(fd, &s->fs.wset);
    }
}

static void reset_fdsets(struct fstate* const f) {
    for (enum channel i = CHANNEL_FIRST; i <= CHANNEL_LAST; i++) {
        const int fd = f->ch[i].fd;
        if (fd < 0)
            continue;
        FD_SET(fd, &f->eset);
        if (i >= CHANNEL_FIRST_INPUT && i <= CHANNEL_LAST_INPUT)
            FD_SET(fd, &f->rset);
    }
}

static int wait_for_io(struct state* const s) {
    struct timeval t = { .tv_sec = s->select_params.timeout_sec };
    reset_fdsets(&s->fs);
    const int l = select(s->select_params.maxfd, &s->fs.rset, &s->fs.wset, &s->fs.eset, (s->select_params.timeout_sec < 0 ? NULL : &t));
    if (l < 0 || (l == 0 && s->select_params.timeout_sec != 0)) {
        if (l == 0) {
            errno = ETIMEDOUT;
            if (s->pings_on_inactivity_left)
                return commission_ping_on_main(s);
        }
        LOGF("select");
    }
    return l;
}

static void event_loop(struct state* const s) {
    struct frob_frame_fsm_state r = { .p = s->fs.ch[CHANNEL_FI_MAIN].buf };
    while (wait_for_io(s)) {
        perform_pending_io(s);
        for (enum channel i = CHANNEL_FIRST_INPUT; i <= CHANNEL_LAST_INPUT; i++)
            if (s->fs.ch[i].fd >= 0 && FD_ISSET(s->fs.ch[i].fd, &s->fs.rset))
                process_channel(i, s, &r);
    }
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

static void ucspi_log(const char* const proto, const char* const connnum) {
    static const char* const rl[] = { "REMOTE", "LOCAL" };
    static const char* const ev[] = { "HOST", "IP", "PORT", "INFO" };

    char tmp[16];
    const char* ed[elementsof(rl)][elementsof(ev)];
    for (size_t j = 0; j < elementsof(rl); j++)
        for (size_t i = 0; i < elementsof(ev); i++)
            ed[j][i] = getenvfx(tmp, sizeof tmp, "%s%s%s", proto, rl[j], ev[i]);

    LOGIX("UCSPI compatible environment detected (%s)", (connnum ? "server" : "client"));
    // FIXME: Fix this madness
    LOGDX("proto: %s; remote: %s%s%s%s%s:%s%s%s%s%s%s%s%s%s%s;",
            proto,
            (ed[0][0] ? "\"" : ""), (ed[0][0] ?: ""), (ed[0][0] ? "\"" : ""),
            (ed[0][0] ? " (" : ""), ed[0][1], ed[0][2], (ed[0][0] ? ")" : ""),
            (connnum ? " connections: " : ""), (connnum ?: ""),
            (ed[0][3] ? " identification: " : ""), (ed[0][3] ?: ""),
            (ed[1][0] ? "; local: ": ""),
            (ed[0][0] ? "\"" : ""), (ed[1][0] ?: ""), (ed[0][0] ? "\"" : "")
    );
}

static const char* ucspi_adjust(const char* const proto, struct fstate* const f) {
    char tmp[16];
    const char* const connnum = getenvfx(tmp, sizeof tmp, "%sCONNNUM", proto);
    // Tested only with s6-networking
    if (!connnum)
        f->ch[CHANNEL_FO_MAIN].fd = (f->ch[CHANNEL_FI_MAIN].fd = 6) + 1;
    return connnum;
}

static void initialize(struct state* const s, const int ac, const char* av[static const ac]) {
    s->pings_on_inactivity_left = 2;
    for (enum channel i = CHANNEL_FIRST; i <= CHANNEL_LAST; i++) {
        s->fs.ch[i].fd = -1;
        s->fs.ch[i].cur = s->fs.ch[i].buf;
    }
    s->sigfdset = adjust_signal_delivery(&s->fs.ch[CHANNEL_II_SIGNAL].fd);
    s->fs.ch[CHANNEL_FO_MAIN].fd = STDOUT_FILENO;
    s->fs.ch[CHANNEL_FI_MAIN].fd = STDIN_FILENO;
    if ((s->fs.ch[CHANNEL_NO_PAYMENT].fd = open("./payment", O_WRONLY | O_CLOEXEC)) == -1)
        LOGI("%s channel not available at %s", channel_to_string(CHANNEL_NO_PAYMENT), "./payment");
    const char* const proto = getenv("PROTO");
    if (proto)
        ucspi_log(proto, ucspi_adjust(proto, &s->fs));
    s->select_params.maxfd = get_max_fd(&s->fs.ch);
    FD_ZERO(&s->fs.eset);
    FD_ZERO(&s->fs.wset);
    FD_ZERO(&s->fs.eset);
    s->select_params = (struct select_params) {
        .timeout_sec = ac == 2 ? atoi(av[1]) : 0,
        .maxfd = get_max_fd(&s->fs.ch),
    };
}

static void adjust_rlimit(void) {
    // This will force syscalls that allocate file descriptors to fail if it
    // doesn't fit into fd_set, so we don't have to check for that in the code.
    struct rlimit rl;
    if (getrlimit(RLIMIT_NOFILE, &rl) != 0)
        LOGF("getrlimit");
    if (rl.rlim_cur > FD_SETSIZE)
        if (setrlimit(RLIMIT_NOFILE, &(struct rlimit){ .rlim_cur = FD_SETSIZE, .rlim_max = rl.rlim_max }) != 0)
            LOGF("setrlimit");
}

int main(const int ac, const char* av[static const ac]) {
    static struct state s = {
        .role = ROLE_EFT,
        .hm = {
            [H_T2] = "T2" FS "170" FS "TEST" FS "SIM" FS "0" FS,
            [H_B2] = "B2" FS "170" FS "TEST" FS "SIM" FS "0" FS "0" FS "0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000" FS "00" FS,
            [H_T4] = "T4" FS "160" US "170" US FS,
            [H_T5] = "T5" FS "170" FS,
            [H_S2] = "S2" FS "993" FS FS "M000" FS "T000" FS "N/A" FS FS FS "NONE" FS "Payment endopoint not available" FS,
            [H_K0] = "K0" FS "0" FS,
            [H_D5] = "D5" FS "24" FS "12" FS "6" FS "19" FS "1" FS "1" FS "1"
                    FS "0" FS "0" FS "0" FS FS FS "4" FS "9999" FS "4" FS "15"
                    FS "ENTER" US "CANCEL" US "CHECK" US "BACKSPACE" US "DELETE" US "UP" US "DOWN" US "LEFT" US "RIGHT" US
                    FS "1" FS "1" FS "1" FS "0" FS
        }
    };
    initialize(&s, ac, av);
    adjust_rlimit();
    event_loop(&s);
    return EXIT_SUCCESS;
}
