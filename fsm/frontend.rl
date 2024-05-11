#include "frob.h"
#include "log.h"
#include "utils.h"
#include <stdbool.h>
#include <sys/timerfd.h>
#include <unistd.h>
#include <npth.h>

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
        LOGDXP(char tmp[4*1], "← % 4d %s", 1, PRETTY(&acknak, &acknak + 1, tmp));
        if (npth_write(6, &acknak, 1) != 1) {
            LOGE("write");
            fbreak;
        }
    }
    action Process {
        LOGDXP(char tmp[4*(pe-p)], "Lending %zd bytes: %s", pe - p, PRETTY((unsigned char*)p, (unsigned char*)pe, tmp));
        //sus_lend(1, pe - p, (char*)p);// TODO: Remove this cast
    }
    action Forward {
        if (npth_write(forwarded_fd, p, pe - p) != pe - p) {
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

static int fsm_exec(const char* p, const char* const pe) {
    unsigned char acknak;
    %% write exec;
    return -1;
}

__attribute__((constructor))
void fsm_frontend_init() {
    (void)frontend_en_main, (void)frontend_error, (void)frontend_first_final;
    %% write init;
}

void* fsm_frontend_foreign(struct fsm_frontend_foreign_args* const a) {
    (void)a;
    ssize_t bytes;
    char buf[1024];
    const char* p;
    while ((bytes = npth_read(a->in, buf, sizeof buf)) >= 0) {
        LOGDX("Received %zd bytes", bytes);
        const char* const pe = (p = buf) + bytes;
        fsm_exec(p, pe);
    }
    if (bytes < 0)
        LOGE("Closing fronted");
    return NULL;
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
    (void)a;
    const int fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    ssize_t bytes;
    unsigned char buf[8];
    while ((bytes = npth_read(fd, buf, sizeof buf)) > 0) {
        const char* p = (char[]){0}, * const pe = p + 1;
        fsm_exec(p, pe);
    }
    return -1;
}
