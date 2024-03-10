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
        LOGDX("frontend: Confirm");
        acknak = 0x06;
    }
    action Reject {
        LOGDX("frontend: Reject");
        acknak = 0x15;
    }
    action Send {
        LOGDX("frontend: Send");
        sus_write(STDOUT_FILENO, &acknak, 1);
    }

    foreign = (OK @Confirm | NAK @Reject) @Send;
    internal = IDEMPOTENT ACK | IDEMPOTENT TIMEOUT{1,3} ACK;

    main := (foreign | internal)*;
}%%

%% write data;

#if 0
static bool is_idempotent(const char* const msg) {
    switch (msg[5]) {
        case 'T':
        case 'P':
            return true;
    }
    return false;
}
#endif

static int fsm_exec(const char* p, const char* const pe) {
    char acknak;
    %% write exec;
    return -1;
}

__attribute__((constructor))
void fsm_frontend_init() {
    (void)frontend_en_main, (void)frontend_error, (void)frontend_first_final;
    %% write init;
}

int fsm_frontend_foreign(struct args_frontend_foreign* const a) {
    (void)a;
    //char acknak = 0x00;
    ssize_t bytes;
    const char* p;
    while ((bytes = sus_borrow(0, (void**)&p)) >= 0) {
        LOGDX("Borrowed frame from 0");
        const char* const pe = p + 1;
        fsm_exec(p, pe);
        sus_return(0);
        LOGDX("Returned frame on 0");
    }
    return -1;
}

int fsm_frontend_internal(struct args_frontend_internal* const a) {
    (void)a;
#   if 0
    ssize_t bytes;
    const char* msg;
    while ((bytes = sus_lend(1, &msg, 0)) > 0) {
        const char* p = (char[]){is_idempotent(msg) ? 0x0A : 0x0D}, * const pe = p + 1;
        fsm_exec(p, pe);
    }
#   endif
    return -1;
}

int fsm_frontend_timer(struct args_frontend_timer* const a) {
    (void)a;
#   if 0
    const int fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    ssize_t bytes;
    unsigned char buf[8];
    while ((bytes = sus_read(fd, buf, sizeof buf)) > 0) {
        const char* p = (char[]){0}, * const pe = p + 1;
        fsm_exec(p, pe);
    }
#   endif
    return -1;
}

int n_fsm_frontend_timer() {
#   if 0
    void coro(void* a) {
        ssize_t bytes;
        unsigned char buf[8];
        while ((bytes = sus_read(fd, buf, sizeof buf)) > 0) {
            const char* p = (char[]){0}, * const pe = p + 1;
            fsm_exec(p, pe);
        }
        return -1;
    }
    coro_construct(32, &coro, NULL);
#   endif
    return -1;
}
