#include "frob.h"
#include "utils.h"
#include "log.h"
#include "multitasking/sus.h"
#include <signal.h>
#include <stdio.h>
#include <sys/signalfd.h>

#ifndef SIGINFO
#   define SIGINFO SIGPWR
#endif

static int create_signalfd(void) {
    static const int blocked_signals[] = { SIGINFO, SIGINT };
    sigset_t s;
    sigemptyset(&s);
    for (size_t i = 0; i < elementsof(blocked_signals); i++)
        sigaddset(&s, blocked_signals[i]);
    xsigprocmask(SIG_BLOCK, &s, NULL);
    return xsignalfd(-1, &s, SFD_CLOEXEC);
}

static char* fsiginfo(const struct signalfd_siginfo* const si, const size_t size, char buf[static const size]) {
    FILE* const s = fmemopen(buf, size, "w");
    setbuf(s, NULL);
    xfprintf(s, "%s", strsignal(si->ssi_signo));
    xfclose(s);
    return buf;
}

static void process_signal(const struct signalfd_siginfo* const si) {
    LOGDXP(char tmp[512], "signal: %s", fsiginfo(si, sizeof tmp, tmp));
}

int sighandler(struct sighandler_args*) {
    const int sfd = create_signalfd();
    struct signalfd_siginfo inf;
    while (sus_read(sfd, &inf, sizeof inf) == sizeof inf)
        process_signal(&inf);
    return -1;
}
