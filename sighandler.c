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

static int setup_signalfd(const int ch, const sigset_t blocked) {
    if (sigprocmask(SIG_BLOCK, &blocked, NULL) != 0)
        EXITF("Couldn't adjust signal mask");
    const int fd = signalfd(ch, &blocked, SFD_CLOEXEC);
    if (fd == -1)
        EXITF("Couldn't setup sigfd for %d", ch);
    return fd;
}

static sigset_t adjust_signal_delivery(int* const ch) {
    static const int blocked_signals[] = { SIGINFO, SIGINT };
    sigset_t s;
    sigemptyset(&s);
    for (size_t i = 0; i < elementsof(blocked_signals); i++)
        sigaddset(&s, blocked_signals[i]);
    *ch = setup_signalfd(*ch, s);
    return s;
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
    int sfd = -1;
    adjust_signal_delivery(&sfd);
    struct signalfd_siginfo inf;
    while (sus_read(sfd, &inf, sizeof inf) == sizeof inf)
        process_signal(&inf);
    return -1;
}
