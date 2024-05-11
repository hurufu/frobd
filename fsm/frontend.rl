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
    alphtype unsigned char;
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

    action Process {
        LOGDXP(char tmp[4*(pe-p)], "Not lending %zd bytes: %s", pe - p, PRETTY(p, pe, tmp));
        //npth_write(-1, pe - p, (char*)p);// TODO: Remove this cast
    }
    action Forward {
        xnpth_write(forwarded_fd, p, pe - p) != pe - p);
    }

    foreign = OK @Process;
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
    %% write exec;
    return -1;
}

__attribute__((constructor))
void fsm_frontend_init() {
    (void)frontend_en_main, (void)frontend_error, (void)frontend_first_final;
    %% write init;
}

void* fsm_frontend_foreign(const struct fsm_frontend_foreign_args* const a) {
    unsigned char buf[1024], * pe, * p;
    while ((pe = (p = buf) + xnpth_read_fancy(a->from_wp, sizeof buf, buf)) != buf)
        fsm_exec(p, pe);
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
