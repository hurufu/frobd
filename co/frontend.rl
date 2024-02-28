#include "comultitask.h"
#include "frob.h"
#include <stdbool.h>
#include <sys/timerfd.h>

%%{
    machine frontend;
    alphtype char;
    include frob_common "../common.rl";
    variable cs *cs;

    # Foreign:
    OK = stx;
    NOK = etx;
    ACK = 0x06;
    NAK = 0x15;
    TIMEOUT = 0;

    # Internal:
    IDEMPOTENT = 0x0A;
    EFFECTFUL = 0x0D;

    action Confirm { acknak = 0x06; }
    action Reject { acknak = 0x15; }
    action Send { sus_write(fdout, &acknak, 1); }

    foreign = (OK @Confirm | NAK @Reject) @Send;
    internal = IDEMPOTENT ACK | IDEMPOTENT TIMEOUT{1,3} ACK;

    main := (foreign | internal)*;
}%%

%% write data;

static bool is_idempotent(const char* const msg) {
    switch (msg[5]) {
        case 'T':
        case 'P':
            return true;
    }
    return false;
}

static int fsm_exec(int* cs, const char* p, const char* const pe) {
    char acknak;
    int fdout = 5;
    %% write exec;
    return -1;
}

void fsm_frontend_init(int* const cs) {
    %% write init;
}

int fsm_frontend_foreign(struct args_frontend_foreign* const a) {
    char acknak = 0x00;
    ssize_t bytes;
    const char* p;
#   if 0
    while ((bytes = sus_lend(a->idfrom, &p, 1)) > 0) {
        const char* const pe = p + 1;
        fsm_exec(&a->cs, p, pe);
    }
#   endif
    return -1;
}

int fsm_frontend_internal(struct args_frontend_internal* const a) {
    ssize_t bytes;
    const char* msg;
#   if 0
    while ((bytes = sus_lend(a->idfrom, &msg, 0)) > 0) {
        const char* p = (char[]){is_idempotent(msg) ? 0x0A : 0x0D}, * const pe = p + 1;
        fsm_exec(&a->cs, p, pe);
    }
#   endif
    return -1;
}

int fsm_frontend_timer(struct args_frontend_timer* const a) {
    const int fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    ssize_t bytes;
    unsigned char buf[8];
#   if 0
    while ((bytes = sus_read(fd, buf, sizeof buf)) > 0) {
        const char* p = (char[]){0}, * const pe = p + 1;
        fsm_exec(&a->cs, p, pe);
    }
#   endif
    return -1;
}

int n_fsm_frontend_timer() {
    int cs;
#   if 0
    void coro(void* a) {
        ssize_t bytes;
        unsigned char buf[8];
        while ((bytes = sus_read(fd, buf, sizeof buf)) > 0) {
            const char* p = (char[]){0}, * const pe = p + 1;
            fsm_exec(&a->cs, p, pe);
        }
        return -1;
    }
    coro_construct(32, &coro, NULL);
#   endif
    return -1;
}
