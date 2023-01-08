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
#include <stdbool.h>

#define SLATE_FOR_DEBUG(Channel, State, Fmt, ...) do {\
    if ((*(Channel))[EW_DEBUG] >= 0) {\
        const int snprintf_ret = snprintf((char*)(State)->cur[EW_DEBUG], endof((State)->buf[EW_DEBUG]) - (State)->cur[EW_DEBUG], "D " Fmt "\n", ##__VA_ARGS__);\
        if (snprintf_ret > 0) {\
            (State)->cur[EW_DEBUG] += snprintf_ret;\
            FD_SET((*Channel)[EW_DEBUG], &((State)->set[FD_WRITE]));\
        }\
    }\
} while (0)

#define PRETTY(Arr) to_printable(Arr, endof(Arr), 4*sizeof(Arr), (char[4*sizeof(Arr)]){})
#define PRETTV(P, Pe, Buf) to_printable(P, Pe, sizeof(Buf), Buf)

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
    EW_DEBUG,

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

int frob_forward_msg(const struct frob_msg* const msg) {
    (void)msg;
    // Not implemented;
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
        case EW_DEBUG:   return "EW_DEBUG";
        case CHANNELS_COUNT:
    }
    return NULL;
}

static const char* fdset_to_string(const enum OrderedFdSets o) {
    switch (o) {
        case FD_EXCEPT: return "FD_EXCEPT";
        case FD_WRITE:  return "FD_WRITE";
        case FD_READ:   return "FD_READ";
        case FD_SET_COUNT:
    }
    return NULL;
}

static char* to_printable(const unsigned char* const p, const unsigned char* const pe,
                                  const size_t s, char b[static const s]) {
    // TODO: Add support for regional characters, ie from 0x80 to 0xFF
    // TODO: Rewrite to_printable using libicu
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
            fprintf(stderr, "Bad payload\n");
            return 2;
    }

    switch (frob_extract_additional_attributes(&p, pe, &msg.attr)) {
        case EBADMSG:
            fprintf(stderr, "Bad data\n");
            return 3;
    }

    //assert(p == pe);

    if (frob_forward_msg(&msg) != 0) {
        fprintf(stderr, "Downstream error\n");
        return 4;
    }

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
    // TODO: Use pselect and enable master channel on SIGINT
    return select(nfd, &(*set)[FD_READ], &(*set)[FD_WRITE], &(*set)[FD_EXCEPT], &t) > 0;
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

static int perform_pending_io(struct io_state* const t, int (*channel)[CHANNELS_COUNT]) {
    for (size_t j = 0; j < elementsof(t->set); j++) {
        for (size_t i = 0; i < elementsof(*channel); i++) {
            if (FD_ISSET((*channel)[i], &(t->set)[j])) {
                switch (j) {
                    ssize_t s;
                    case FD_EXCEPT:
                        errno = EBADE;
                        return -2;
                    case FD_WRITE:
                        if ((s = write((*channel)[i], t->buf[i], t->cur[i] - t->buf[i])) != t->cur[i] - t->buf[i])
                            return -4;
                        t->cur[i] = t->buf[i];
                        FD_CLR((*channel)[i], &(t->set)[FD_WRITE]);
                        break;
                    case FD_READ:
                        if ((s = read((*channel)[i], t->cur[i], t->buf[i] + sizeof t->buf[i] - t->cur[i])) < 0) {
                            return -3;
                        } else if (s == 0) {
                            close((*channel)[i]);
                            (*channel)[i] = -1;
                        }
                        t->cur[i] += s;
                        break;
                }
            }
        }
    }
    return 0;
}

static int event_loop(int (* const channel)[CHANNELS_COUNT], const time_t relative_timeout) {
    static struct io_state t;

    const int m = get_max_fd(channel);
    struct frob_frame_fsm_state f = { .p = t.buf[ER_MAIN] };
    for (finit(channel, &t); fselect(m, &t.set, relative_timeout); fset(channel, &t.set)) {
        const int r = perform_pending_io(&t, channel);
        if (r != 0)
            return r;

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
                SLATE_FOR_DEBUG(channel, &t, "%s → %s", PRETTV(f.p, f.pe, tmp), PRETTY(ack));
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
        [EW_DEBUG  ] = STDERR_FILENO,
        [ER_MASTER ] = -1
    };

    if (!all_channels_ok(&channel))
        return EXIT_FAILURE;

    event_loop(&channel, 1);
    perror("Exit");
    return EXIT_FAILURE;
}
