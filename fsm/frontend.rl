#include "multitasking/sus.h"
#include "frob.h"
#include "log.h"
#include "utils.h"
#include <stdbool.h>
#include <unistd.h>

static int cs;

%%{
    machine frontend;
    alphtype char;
    include frob_common "common.rl";

    # Foreign:
    OK = stx;
    NOK = etx;
    ACK = 0x06;
    NAK = 0x15;
    TIMEOUT = 0;

    # Internal:
    IDEMPOTENT = 0x0A;
    EFFECTFUL = 0x0D;

    action Confirm {
        acknak = 0x06;
    }
    action Reject {
        acknak = 0x15;
    }
    action Send {
        if (sio_write(a->foreign_out, &acknak, 1) != 1) {
            LOGE("write");
            fbreak;
        }
        LOGDXP(char tmp[4*1], "← % 4d %s", 1, PRETTY(&acknak, &acknak + 1, tmp));
    }
    action Process {
        LOGDXP(char tmp[4*(pe-p)], "to_autoresponder %zd bytes: %s", pe - p, PRETTY((unsigned char*)p, (unsigned char*)pe, tmp));
        sio_write(a->to_autoresponder, p, pe - p);
    }
    action Forward {
        if (sio_write(forwarded_fd, p, pe - p) != pe - p) {
            LOGE("write");
            fbreak;
        }
    }

    foreign = OK @Confirm @Send @Process | NAK @Reject @Send;
    internal = IDEMPOTENT ACK | IDEMPOTENT TIMEOUT{1,3} ACK;

    main := (foreign | internal)*;
}%%

%% write data;

/*
static bool is_idempotent(const char* const msg) {
    switch (msg[5]) {
        case 'T':
        case 'P':
            return true;
    }
    return false;
}
*/

static int fsm_exec(struct fsm_frontend_foreign_args* const a, const char* p, const char* const pe) {
    unsigned char acknak;
    %% write exec;
    return -1;
}

__attribute__((constructor))
void fsm_frontend_init() {
    (void)frontend_en_main, (void)frontend_error, (void)frontend_first_final;
    %% write init;
}

int fsm_frontend_foreign(struct fsm_frontend_foreign_args* const a) {
    (void)a;
    ssize_t bytes;
    char buf[1024];
    const char* p = buf;
    while ((bytes = sio_read(a->infd, buf, sizeof buf)) > 0) {
        LOGDX("Received %zd bytes", bytes);
        const char* const pe = (p = buf) + bytes;
        fsm_exec(a, p, pe);
    }
    LOGE("Closing fronted");
    close(a->infd);
    return -1;
}

int fsm_frontend_internal(struct fsm_frontend_internal_args* const a) {
    (void)a;
/*
    ssize_t bytes;
    const char* msg;
    while ((bytes = sus_lend(_, &msg, 0)) > 0) {
        const char* p = (char[]){is_idempotent(msg) ? 0x0A : 0x0D}, * const pe = p + 1;
        fsm_exec(p, pe);
    }
*/
    return -1;
}

int fsm_frontend_timer(struct fsm_frontend_timer_args* const a) {
    /*
    (void)a;
    const int fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    set_nonblocking(fd);
    ssize_t bytes;
    unsigned char buf[8];
    while ((bytes = sio_read(fd, buf, sizeof buf)) > 0) {
        const char* p = (char[]){0}, * const pe = p + 1;
        fsm_exec(p, pe);
    }
    */
    return -1;
}
