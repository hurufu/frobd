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

#if 0
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
#endif

void fsm_frontend_init(int* const cs) {
    (void)frontend_en_main, (void)frontend_error, (void)frontend_first_final;
    %% write init;
}

int fsm_frontend_foreign(struct args_frontend_foreign* const a) {
    (void)a;
#   if 0
    char acknak = 0x00;
    ssize_t bytes;
    const char* p;
    while ((bytes = sus_lend(a->idfrom, &p, 1)) > 0) {
        const char* const pe = p + 1;
        fsm_exec(&a->cs, p, pe);
    }
#   endif
    return -1;
}

int fsm_frontend_internal(struct args_frontend_internal* const a) {
    (void)a;
#   if 0
    ssize_t bytes;
    const char* msg;
    while ((bytes = sus_lend(1, &msg, 0)) > 0) {
        const char* p = (char[]){is_idempotent(msg) ? 0x0A : 0x0D}, * const pe = p + 1;
        fsm_exec(&a->cs, p, pe);
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
        fsm_exec(&a->cs, p, pe);
    }
#   endif
    return -1;
}

int n_fsm_frontend_timer() {
#   if 0
    int cs;
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
