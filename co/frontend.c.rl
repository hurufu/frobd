#include "comultitask.h"
#include "frob.h"
#include "../log.h"
#include <stdbool.h>
#include <sys/timerfd.h>
#include <unistd.h>

static int cs;

%%{
    machine frontend;
    alphtype char;
    include frob_common "../common.rl";

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
        LOGDX("Confirm");
        acknak = 0x06;
    }
    action Reject {
        LOGDX("Reject");
        acknak = 0x15;
    }
    action Send {
        LOGDX("Send %#04x", acknak);
        if (sus_write(STDOUT_FILENO, &acknak, 1) != 1) {
            LOGE("write");
            fbreak;
        }
    }

    foreign = (OK @Confirm | NAK @Reject) @Send;
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
    char acknak;
    %% write exec;
    return -1;
}

__attribute__((constructor))
void fsm_frontend_init() {
    (void)frontend_en_main, (void)frontend_error, (void)frontend_first_final;
    set_nonblocking(STDOUT_FILENO);
    %% write init;
}

int fsm_frontend_foreign(struct args_frontend_foreign* const a) {
    (void)a;
    //char acknak = 0x00;
    ssize_t bytes;
    const char* p;
    while ((bytes = sus_borrow(0, (void**)&p)) >= 0) {
        const char* const pe = p + 1;
        fsm_exec(p, pe);
        sus_return(0, p, bytes);
    }
    if (bytes < 0)
        LOGE("Closing fronted");
    return -1;
}

int fsm_frontend_internal(struct args_frontend_internal* const a) {
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

int fsm_frontend_timer(struct args_frontend_timer* const a) {
    (void)a;
/*
    const int fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    ssize_t bytes;
    unsigned char buf[8];
    while ((bytes = sus_read(_, buf, sizeof buf)) > 0) {
        const char* p = (char[]){0}, * const pe = p + 1;
        fsm_exec(p, pe);
    }
*/
    return -1;
}

int n_fsm_frontend_timer() {
/*
    void coro(void* a) {
        ssize_t bytes;
        unsigned char buf[8];
        while ((bytes = sus_read(_, buf, sizeof buf)) > 0) {
            const char* p = (char[]){0}, * const pe = p + 1;
            fsm_exec(p, pe);
        }
        return -1;
    }
    coro_construct(32, &coro, NULL);
*/
    return -1;
}
