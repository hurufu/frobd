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
#include <limits.h>
#include <time.h>

#ifndef IO_BUF_SIZE
#   define IO_BUF_SIZE (4 * 1024)
#endif

// Don't reorder!
enum channel {
    CHANNEL_NONE = -1,

    CHANNEL_NO_PAYMENT,
    CHANNEL_NO_STORAGE,
    CHANNEL_NO_UI,
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

struct config {
    struct timeouts {
        unsigned short ack, // Timeout for ACK/NAK
                ping,       // Timeout for T2
                payment,    // Timeout for "action during payment" (sic)
                response;   // Timeout for any other message
        short inactivity;   // Timeout for inactivity
    } timeout;
    version_t supported_versions[4];
    struct frob_device_info info;
    struct frob_d5 parameters;
    char fallback_s2[64];
};

struct state {
    struct config cfg;

    struct statistics {
        unsigned short received_good,
                received_bad_lrc,
                received_bad_parse,
                received_bad_handled;
    } stats;

    // Signals to be handled by signalfd
    sigset_t sigfdset;

    bool ack;
    bool ping;
    bool busy;

    unsigned short pings_on_inactivity_left;

    timer_t timer_ack;
    timer_t timer_ping;

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
    assert(false);
    return NULL;
}

static char channel_to_code(const enum channel o) {
    switch (o) {
        case CHANNEL_NO_PAYMENT: return 'P';
        case CHANNEL_NO_STORAGE: return 'S';
        case CHANNEL_NO_UI:      return 'U';
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
    assert(false);
    return '?';
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
        return LOGEX("No destination for message %s: %s", frob_type_to_string(msg->header.type), strerror(ECHRNG)), -1;
    if (f->ch[dst].fd == -1) {
        LOGWX("Channel %s is not connected: %s", channel_to_string(dst), strerror(EUNATCH));
        dst = CHANNEL_FO_MAIN;
    }
    return dst;
}

static enum FrobMessageType choose_hardcoded_response(const enum FrobMessageType t) {
    switch (t) {
        case FROB_T1: return FROB_T2;
        case FROB_T3: return FROB_T4;
        case FROB_T4: return FROB_T5;
        case FROB_D4: return FROB_D5;
        case FROB_B1: return FROB_B2;
        case FROB_S1: return FROB_S2;
        case FROB_K1: return FROB_K0;
        case FROB_T2:
        case FROB_T5:
            return FROB_NONE;
        default:
            LOGWX("No hardcoded response for %s (%#x)", frob_type_to_string(t), t);
            break;
    }
    // All messages must be handled
    assert(false);
    return FROB_NONE;
}

static size_t free_space(const struct chstate* const ch) {
    assert(ch->cur >= ch->buf);
    assert(ch->cur <= lastof(ch->buf));
    return lastof(ch->buf) - ch->cur;
}

static size_t used_space(const struct chstate* const ch) {
    assert(ch->cur >= ch->buf);
    assert(ch->cur <= lastof(ch->buf));
    return ch->cur - ch->buf;
}

static union frob_body construct_hardcoded_message_body(const struct config* const cfg, const enum FrobMessageType t) {
    union frob_body ret = {};
    switch (t) {
        case FROB_T1:
        case FROB_T3:
        case FROB_D4:
        case FROB_P1:
        case FROB_A1:
        case FROB_K1:
            break;
        case FROB_T2:
            ret.t2.info = cfg->info;
            break;
        case FROB_B1:
            ret.b1.info = cfg->info;
            break;
        case FROB_B2:
            ret.b2.info = cfg->info;
            break;
        case FROB_T4:
            memcpy(ret.t4.supported_versions, cfg->supported_versions, sizeof cfg->supported_versions);
            break;
        case FROB_T5:
            memcpy(ret.t5.selected_version, cfg->info.version, sizeof cfg->info.version);
            break;
        case FROB_D5:
            ret.d5 = cfg->parameters;
            break;
        default:
            LOGEX("No hardcoded body for %s (%#x)", frob_type_to_string(t), t);
            assert(false);
    }
    return ret;
}

// FIXME: Rewrite this function, so it wouldn't use condtion on device type
static token_t compute_next_token(const enum FrobDeviceType r) {
    static token_t token = 0;
    // Specification doesn't define what to do in case of token overflow
    return token = (token + 1) % (r == FROB_DEVICE_TYPE_ECR ? 10000 : 20000);
}

static int commission_native_message(struct fstate* const f, const enum channel dst, const struct frob_msg* const msg) {
    // Magic string should match
    assert(strcmp(msg->magic, FROB_MAGIC) == 0);

    struct chstate* const ch = &f->ch[dst];
    if (sizeof *msg >= free_space(ch))
        return LOGEX("Message skipped: %s", strerror(ENOBUFS)), -1;
    memcpy(ch->cur, msg, sizeof *msg);

    FD_SET(ch->fd, &f->wset);
    ch->cur += sizeof *msg;
    return 0;
}

static int commission_frame_on_main(struct state* const st, const token_t t, const char* const body) {
    struct fstate* const f = &st->fs;
    struct chstate* const ch = &f->ch[CHANNEL_FO_MAIN];
    const size_t s = free_space(ch);
    const int ret = snprintf((char*)ch->cur, s, STX "%" PRIXTOKEN FS "%s" ETX "_", t, body);
    if (ret < 0)
        EXITF("Can't place frame");
    if ((unsigned)ret >= s || ret == 0)
        return LOGEX("Frame skipped: %s", (ret ? strerror(ENOBUFS): "Empty message")), -1;
    ch->cur[ret - 1] = calculate_lrc(ch->cur + 1, ch->cur + ret - 1);

    st->ack = true;
    FD_SET(ch->fd, &f->wset);

    // Parseable frame is created
    assert(frob_frame_process(&(struct frob_frame_fsm_state){.p = ch->cur, .pe = ch->cur + s}) == 0);
    // Parseable message within that frame is created
    assert(parse_message(ch->cur + 1, ch->cur + ret - 2, &(struct frob_msg){ .magic = FROB_MAGIC }) == 0);

    ch->cur += ret;
    return 0;
}

static int commission_hardcoded_message_on_main(struct state* const s, const token_t t, const enum FrobMessageType m) {
    struct chstate* const main = &s->fs.ch[CHANNEL_FO_MAIN];
    const struct frob_msg response = {
        .magic = FROB_MAGIC,
        .header = {
            .token = t,
            .type = m
        },
        .body = construct_hardcoded_message_body(&s->cfg, m)
    };
    const ssize_t w = serialize(free_space(main), main->cur, &response);
    if (w < 0)
        return LOGEX("Message skipped"), -1;

    s->ack = true;
    FD_SET(main->fd, &s->fs.wset);

    main->cur += w;
    return 0;
}

static int commission_response(struct state* const s, const struct frob_msg* const received_msg) {
    enum channel dst = choose_destination(&s->fs, received_msg);
    if (dst == CHANNEL_FO_MAIN) {
        if (received_msg->header.type == FROB_T2)
            s->pings_on_inactivity_left++;
        const enum FrobMessageType resp = choose_hardcoded_response(received_msg->header.type);
        switch (resp) {
            case FROB_NONE:
                return 0;
            case FROB_S2:
                return commission_frame_on_main(s, received_msg->header.token, s->cfg.fallback_s2);
            default:
                break;
        }
        // Reply with hardcoded response
        return commission_hardcoded_message_on_main(s, received_msg->header.token, resp);
    }
    // Forward message without changing it
    return commission_native_message(&s->fs, dst, received_msg);
};

static void commission_prompt_on_master(struct state* const st) {
    MCOPY(st->fs.ch[CHANNEL_CO_MASTER].cur, "> ");
    FD_SET(st->fs.ch[CHANNEL_CO_MASTER].fd, &st->fs.wset);
}

static int commission_ping_on_main(struct state* const s) {
    if (s->fs.ch[CHANNEL_FO_MAIN].fd < 0)
        return 0;
    const int ret = commission_frame_on_main(s, compute_next_token(s->cfg.parameters.device_topo), "T1" FS) == 0;
    if (ret)
        s->ping = true;
    return ret;
}

static void commission_ack_on_main(struct state* const s, const bool is_nak) {
    *s->fs.ch[CHANNEL_FO_MAIN].cur++ = is_nak ? NAK : ACK ;
    FD_SET(s->fs.ch[CHANNEL_FO_MAIN].fd, &s->fs.wset);
}

static int setup_signalfd(const int ch, const sigset_t blocked) {
    if (sigprocmask(SIG_BLOCK, &blocked, NULL) != 0)
        EXITF("Couldn't adjust signal mask");
    const int fd = signalfd(ch, &blocked, SFD_CLOEXEC);
    if (fd == -1)
        EXITF("Couldn't setup sigfd for %d", ch);
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
        EXITF("Can't unblock received singal");

    MCOPY(s->fs.ch[CHANNEL_CO_MASTER].cur, "Press ^C again to exit the program or ^D end interactive session...\n");
    commission_prompt_on_master(s);
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

static void alarm_set(timer_t timer, const int sec) {
    const struct itimerspec new = {
        .it_interval = { .tv_sec = 0, .tv_nsec = 0 },
        .it_value = { .tv_sec = sec, .tv_nsec = 0 }
    };
    struct itimerspec old;
    if (timer_settime(timer, 0, &new, &old) != 0)
        EXITF("Can't set timer");

    // We shouldn't set duplicated alrams
    assert(sec ? old.it_value.tv_sec == 0 && old.it_value.tv_nsec == 0 : 1);
}

static void close_main_channel(struct state* const s) {
    close(s->fs.ch[CHANNEL_FO_MAIN].fd);
    close(s->fs.ch[CHANNEL_FI_MAIN].fd);
    s->fs.ch[CHANNEL_FO_MAIN].fd = s->fs.ch[CHANNEL_FI_MAIN].fd = -1;
    // If main channel is closed by remote side then the only
    // meaningful thing we can do is to exit gracefully after all
    // pending writes are done by setting timeout to a small value.
    if (s->select_params.timeout_sec != 0)
        s->select_params.timeout_sec = 1;
}

static void process_signal(struct state* const s) {
    struct chstate* const ch = &s->fs.ch[CHANNEL_II_SIGNAL];

    // Sanity checks, before casting raw buffer to array of struct signalfd_siginfo
    assert(used_space(ch) > 0 && used_space(ch) % sizeof (struct signalfd_siginfo) == 0);

    const size_t n = used_space(ch) / sizeof (struct signalfd_siginfo);
    const struct signalfd_siginfo (* const inf)[n] = (struct signalfd_siginfo(*)[])&ch->buf;
    uintmax_t handled = 0;
    for (size_t i = 0; i < n; i++) {
        const uint32_t signo = (*inf)[i].ssi_signo;

        assert(signo == SIGINT || signo == SIGALRM || signo == SIGPWR);
        assert(sizeof (uintmax_t) * CHAR_BIT > signo);

        const uintmax_t m = 1 << signo;
        if (handled & m)
            continue;
        handled |= m;
        switch (signo) {
            case SIGINT:
                start_master_channel(s);
                break;
            case SIGALRM:
                if (s->fs.ch[CHANNEL_FO_MAIN].fd == -1)
                    break;
                if ((*inf)[i].ssi_tid == (uintmax_t)s->timer_ack) {
                    FD_SET(s->fs.ch[CHANNEL_FO_MAIN].fd, &s->fs.wset);
                } else if ((*inf)[i].ssi_tid == (uintmax_t)s->timer_ping) {
                    LOGWX("Response to ping timed out. Assuming connection is broken");
                    close_main_channel(s);
                }
                break;
            case SIGPWR:
                print_stats(&s->stats);
                break;
            case SIGUSR2:
                s->busy = !s->busy;
                break;
        }
    }
    ch->cur = ch->buf;
}

static void process_master(struct state* const s) {
    LOGWX("No commands are currently supported: %s", strerror(ENOSYS));
    commission_prompt_on_master(s);
}

static struct frob_frame_fsm_state fnext(byte_t* const cursor, const struct frob_frame_fsm_state prev) {
    assert(prev.pe + 2 <= cursor);
    return (struct frob_frame_fsm_state){
        .p = prev.pe + 2, // Skip ETX and LRC
        .pe = cursor
    };
}

static void process_main(struct state* const s, struct frob_frame_fsm_state* const f) {
    int e;
    struct chstate* const ch = &s->fs.ch[CHANNEL_FI_MAIN];
    for (f->pe = ch->cur; (e = frob_frame_process(f)) != EAGAIN; *f = fnext(ch->cur, *f)) {

        // Cursor shall be within the buffer
        assert(ch->cur >= ch->buf && ch->cur < ch->buf + sizeof (ch->buf));

        // Message shall be fully contained by cursor
        assert(f->p >= ch->buf && f->p < ch->cur);
        assert(f->pe >= ch->buf && f->pe <= ch->cur);

        commission_ack_on_main(s, e);
        if (0 == e) {
            // Message length shall be positive
            assert(f->pe > f->p);

            struct frob_msg parsed = { .magic = FROB_MAGIC };
            if (parse_message(f->p, f->pe, &parsed) != 0) {
                LOGWX("Can't process message");
                s->stats.received_bad_parse++;
            } else {
                if (parsed.header.type == FROB_T2) {
                    s->ping = false;
                    alarm_set(s->timer_ping, 0);
                }
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
    f->p = ch->cur = ch->buf;
    f->pe = NULL;
}

static void process_device(struct state* const s) {
    struct chstate* const dev = &s->fs.ch[CHANNEL_NI_DEVICE], * const main = &s->fs.ch[CHANNEL_FO_MAIN];

    assert(used_space(dev) > 0 && used_space(dev) % sizeof (struct frob_msg) == 0);

    const size_t n = used_space(dev) / sizeof (struct frob_msg);
    const struct frob_msg (* const msgs)[n] = (struct frob_msg(*)[])&dev->buf;
    for (size_t i = 0; i < n; i++) {
        if (serialize(free_space(main), main->cur, msgs[i]) < 0) {
            LOGEX("Message skipped");
        }
    }
}

static void process_channel(const enum channel c, struct state* const s, struct frob_frame_fsm_state* const r) {
    switch (c) {
        case CHANNEL_II_SIGNAL: return process_signal(s);
        case CHANNEL_CI_MASTER: return process_master(s);
        case CHANNEL_FI_MAIN:   return process_main(s, r);
        case CHANNEL_NI_DEVICE: return process_device(s);
        default:
            break;
    }
    assert(false);
}

static void perform_pending_write(const enum channel i, struct state* const st) {
    struct chstate* const ch = &st->fs.ch[i];

    // We shouldn't attempt to write 0 bytes
    assert(ch->cur > ch->buf);
    // We can write only to output channels
    switch (i) {
        case CHANNEL_FIRST_OUTPUT ... CHANNEL_LAST_OUTPUT:
            break;
        default:
            assert(false);
    }
    const ptrdiff_t l = used_space(ch);
    const ssize_t s = write(ch->fd, ch->buf, l);
    if (s != l) {
        EXITF("Can't write %td bytes to %s channel (fd %d)", l, channel_to_string(i), ch->fd);
    } else {
        if (i == CHANNEL_FO_MAIN) {
            if (st->ack)
                alarm_set(st->timer_ack, 3);
            if (st->ping)
                alarm_set(st->timer_ping, 10);
        }
        if (i != CHANNEL_CO_MASTER) {
            char tmp[3*s];
            LOGDX("← %c %zu\t%s", channel_to_code(i), s, PRETTV(ch->buf, ch->cur, tmp));
        }
        ch->cur = ch->buf;
    }
}

static void perform_pending_read(const enum channel i, struct state* const s) {
    struct chstate* const ch = &s->fs.ch[i];
    const ssize_t r = read(ch->fd, ch->cur, free_space(ch));
    if (r < 0) {
        EXITF("Can't read data on %s channel (fd %d)", channel_to_string(i), ch->fd);
    } else if (r == 0) {
        LOGIX("Channel %s (fd %d) was closed", channel_to_string(i), ch->fd);
        close(ch->fd);
        FD_CLR(ch->fd, &s->fs.rset);
        ch->fd = -1;
        switch (i) {
            case CHANNEL_FI_MAIN:
                close_main_channel(s);
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
                    alarm_set(s->timer_ack, 0);
                    s->ack = false;
                    s->fs.ch[CHANNEL_FO_MAIN].cur = s->fs.ch[CHANNEL_FO_MAIN].buf;
                    break;
                default:
                    break;
            }
        }
        if (i != CHANNEL_CI_MASTER) {
            char tmp[3*r];
            LOGDX("→ %c %zu\t%s", channel_to_code(i), r, PRETTV(ch->cur, ch->cur + r, tmp));
        }
    }
    ch->cur += r;
}

static void perform_pending_io(struct state* const s) {
    for (enum channel i = CHANNEL_FIRST; i <= CHANNEL_LAST; i++) {
        const int fd = s->fs.ch[i].fd;
        if (fd < 0)
            continue;
        if (FD_ISSET(fd, &s->fs.eset))
            EXITFX("Exceptional data isn't supported");
        if (FD_ISSET(fd, &s->fs.wset))
            perform_pending_write(i, s);
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
            if (s->pings_on_inactivity_left--)
                return commission_ping_on_main(s);
        }
        EXITF("select");
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
        SIGUSR2,
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

    char buf[512];
    const char* ed[elementsof(rl)][elementsof(ev)];
    for (size_t j = 0; j < elementsof(rl); j++)
        for (size_t i = 0; i < elementsof(ev); i++)
            ed[j][i] = getenvfx(buf, sizeof buf, "%s%s%s", proto, rl[j], ev[i]);

    LOGIX("UCSPI compatible environment detected (%s)", (connnum ? "server" : "client"));
    char* p = buf;
    p += snprintfx(p, buf + sizeof buf - p, "proto: %s;", proto);
    for (size_t j = 0; j < elementsof(rl); j++) {
        if (!(ed[j][0] || ed[j][1] || ed[j][2] || ed[j][3]))
            continue;
        p += snprintfx(p, buf + sizeof buf - p, " %s:", rl[j]);
        if (j == 0 && connnum)
            p += snprintfx(p, buf + sizeof buf - p, " #%s", connnum);
        if (ed[j][0])
            p += snprintfx(p, buf + sizeof buf - p, " \"%s\"", ed[j][0]);
        if (ed[j][1] && ed[j][2])
            p += snprintfx(p, buf + sizeof buf - p, " (%s:%s)", ed[j][1], ed[j][2]);
        if (ed[j][3])
            p += snprintfx(p, buf + sizeof buf - p, " [%s]", ed[j][3]);
        p += snprintfx(p, buf + sizeof buf - p, ";");
    }
    LOGDX("%s", buf);
}

static const char* ucspi_adjust(const char* const proto, struct fstate* const f) {
    char tmp[16];
    const char* const connnum = getenvfx(tmp, sizeof tmp, "%sCONNNUM", proto);
    // Tested only with s6-networking
    if (!connnum)
        f->ch[CHANNEL_FO_MAIN].fd = (f->ch[CHANNEL_FI_MAIN].fd = 6) + 1;
    return connnum;
}

static struct frob_d5 load_d5_from_file(const char* const name) {
    input_t buf[256];
    struct frob_msg msg = { .magic = FROB_MAGIC, .header.type = FROB_D5 };
    if (parse_message(buf, buf + slurpx(name, sizeof buf, buf), &msg) != 0)
        EXITFX("parse %s", name);
    return msg.body.d5;
}

static void initialize(struct state* const s, const int ac, const char* av[static const ac]) {
    *s = (struct state){
        .cfg = {
            .fallback_s2 = "S2" FS "993" FS FS "M000" FS "T000" FS "N/A" FS FS FS "NONE" FS "Payment endopoint not available" FS,
            .supported_versions = { "160", "170" },
            .info = {
                .version = "170",
                .vendor = "TEST",
                .device_type = "SIM",
                .device_id = "0"
            },
            .parameters = load_d5_from_file("d5.txt")
        },
        .pings_on_inactivity_left = 2,
        .select_params = {
            .timeout_sec = ac == 2 ? atoi(av[1]) : 0
        }
    };
    for (enum channel i = CHANNEL_FIRST; i <= CHANNEL_LAST; i++) {
        s->fs.ch[i].fd = -1;
        s->fs.ch[i].cur = s->fs.ch[i].buf;
    }
    s->fs.ch[CHANNEL_FO_MAIN].fd = STDOUT_FILENO;
    s->fs.ch[CHANNEL_FI_MAIN].fd = STDIN_FILENO;
    s->sigfdset = adjust_signal_delivery(&s->fs.ch[CHANNEL_II_SIGNAL].fd);
    if ((s->fs.ch[CHANNEL_NO_PAYMENT].fd = open("./payment", O_WRONLY | O_CLOEXEC)) == -1)
        LOGI("%s channel not available at %s", channel_to_string(CHANNEL_NO_PAYMENT), "./payment");
    const char* const proto = getenv("PROTO");
    if (proto)
        ucspi_log(proto, ucspi_adjust(proto, &s->fs));
    s->select_params.maxfd = get_max_fd(&s->fs.ch);
    FD_ZERO(&s->fs.eset);
    FD_ZERO(&s->fs.wset);
    FD_ZERO(&s->fs.eset);
    if (timer_create(CLOCK_MONOTONIC, NULL, &s->timer_ack) != 0)
        EXITF("timer_create");
    if (timer_create(CLOCK_MONOTONIC, NULL, &s->timer_ping) != 0)
        EXITF("timer_create");
}

static void adjust_rlimit(void) {
    // This will force syscalls that allocate file descriptors to fail if it
    // doesn't fit into fd_set, so we don't have to check for that in the code.
    struct rlimit rl;
    if (getrlimit(RLIMIT_NOFILE, &rl) != 0)
        EXITF("getrlimit");
    if (rl.rlim_cur > FD_SETSIZE)
        if (setrlimit(RLIMIT_NOFILE, &(struct rlimit){ .rlim_cur = FD_SETSIZE, .rlim_max = rl.rlim_max }) != 0)
            EXITF("setrlimit");
}

int main(const int ac, const char* av[static const ac]) {
    static struct state s;
    initialize(&s, ac, av);
    adjust_rlimit();
    event_loop(&s);
    return EXIT_SUCCESS;
}
