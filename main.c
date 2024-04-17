#include "log.h"
#include "utils.h"
#include "multitasking/sus.h"
#include "frob.h"
#include <unistd.h>
#include <signal.h>
#include <sys/signalfd.h>

enum channel {
    CHANNEL_NONE = -1,

    CHANNEL_NO_PAYMENT,
    CHANNEL_NO_STORAGE,
    CHANNEL_NO_UI,
    CHANNEL_CO_MASTER,
    CHANNEL_FO_MAIN,

    CHANNEL_II_SIGNAL,
    CHANNEL_CI_MASTER,
    CHANNEL_NI_DEVICE,
    CHANNEL_FI_MAIN,

    CHANNEL_COUNT,

    CHANNEL_FIRST_INPUT  = CHANNEL_II_SIGNAL,
    CHANNEL_FIRST_OUTPUT = CHANNEL_NO_PAYMENT,
    CHANNEL_LAST_INPUT   = CHANNEL_FI_MAIN,
    CHANNEL_LAST_OUTPUT  = CHANNEL_FO_MAIN,
    CHANNEL_FIRST        = CHANNEL_NO_PAYMENT,
    CHANNEL_LAST         = CHANNEL_FI_MAIN
};

static const char* channel_to_string(const enum channel o) {
    assert(o >= CHANNEL_FIRST && o <= CHANNEL_LAST);
    switch (o) {
        case CHANNEL_NO_PAYMENT: return "PAYMENT (native output)";
        case CHANNEL_NO_STORAGE: return "STORAGE (native output)";
        case CHANNEL_NO_UI:      return "UI (native output)";
        case CHANNEL_NI_DEVICE:  return "DEVICE (native input)";
        case CHANNEL_FO_MAIN:    return "MAIN (foreign output)";
        case CHANNEL_FI_MAIN:    return "MAIN (foreign input)";
        case CHANNEL_CI_MASTER:  return "MASTER (console input)";
        case CHANNEL_CO_MASTER:  return "MASTER (console output)";
        case CHANNEL_II_SIGNAL:  return "SIGFD (internal input)";
        case CHANNEL_NONE:
        case CHANNEL_COUNT:
            break;
    }
    return NULL;
}

static void ucspi_log(const char* const proto, const char* const connnum) {
    static const char* const rl[] = { "REMOTE", "LOCAL" };
    static const char* const ev[] = { "HOST", "IP", "PORT", "INFO" };
    static const char* const sn[] = {
        "SSL_PROTOCOL", "SSL_CIPHER", "SSL_TLS_SNI_SERVERNAME",
        "SSL_PEER_CERT_HASH", "SSL_PEER_CERT_SUBJECT" };

    char buf[1024];
    const char* ed[lengthof(rl)][lengthof(ev)];
    for (size_t j = 0; j < lengthof(rl); j++)
        for (size_t i = 0; i < lengthof(ev); i++)
            ed[j][i] = getenvfx(buf, sizeof buf, "%s%s%s", proto, rl[j], ev[i]);

    const char* sv[lengthof(sn)];
    for (size_t k = 0; k < lengthof(sn); k++)
        sv[k] = getenv(sn[k]);

    LOGIX("UCSPI compatible environment detected (%s)", (connnum ? "server" : "client"));
    char* p = buf;
    p += snprintfx(p, buf + sizeof buf - p, "proto: %s;", proto);
    for (size_t j = 0; j < lengthof(rl); j++) {
        if (!(ed[j][0] || ed[j][1] || ed[j][2] || ed[j][3]))
            continue;
        p += snprintfx(p, buf + sizeof buf - p, " %s:", rl[j]);
        if (j == 0 && connnum)
            p += snprintfx(p, buf + sizeof buf - p, " #%s", connnum);
        if (ed[j][0])
            p += snprintfx(p, buf + sizeof buf - p, " \"%s\"", ed[j][0]);
        if (ed[j][1] && ed[j][2])
            p += snprintfx(p, buf + sizeof buf - p, " (%s:%s)", ed[j][1], ed[j][2]);
        if (ed[j][3])
            p += snprintfx(p, buf + sizeof buf - p, " [%s]", ed[j][3]);
        p += snprintfx(p, buf + sizeof buf - p, ";");
    }
    if (sv[0]) {
        p += snprintfx(p, buf + sizeof buf - p, "%s %s %s %s", sv[0], sv[1], sv[3], sv[4]);
        if (sv[2])
            snprintfx(p, buf + sizeof buf - p, " \"%s\"", sv[2]);
    }
    LOGDX("%s", buf);
}

static const char* ucspi_adjust(const char* const proto, int* restrict const in, int* restrict const out) {
    char tmp[16];
    const char* const connnum = getenvfx(tmp, sizeof tmp, "%sCONNNUM", proto);
    // Tested only with s6-networking
    if (!connnum)
        *in = (*out = 6) + 1;
    return connnum;
}

static void ucsp_info_and_adjust_fds(int* restrict const in, int* restrict const out) {
    const char* const proto = getenv("PROTO");
    if (proto)
        ucspi_log(proto, ucspi_adjust(proto, in, out));
}

static ssize_t test_channel(const enum channel c, const int fd) {
    switch (c) {
        case CHANNEL_FIRST_INPUT ... CHANNEL_LAST_INPUT:
            return read(fd, NULL, 0);
        case CHANNEL_FIRST_OUTPUT ... CHANNEL_LAST_OUTPUT:
            return write(fd, NULL, 0);
        default:
            return 0;
    }
}

// We rely on the UCSPI environment (if detected) to provide us usable file
// descriptors. But some programs (e.g. mull-runner) keep environment variables,
// but because they fork() we loose the file descriptors. So we need to test
// them. Important: we need to test all channels before we set signalfd, because
// read of zero bytes from it will always result in EINVAL.
static void test_all_channels(const int (* const fd)[CHANNEL_COUNT]) {
    for (enum channel i = CHANNEL_FIRST; i <= CHANNEL_LAST; i++)
        if ((*fd)[i] >= 0 && test_channel(i, (*fd)[i]) != 0)
            LOGE("Channel %s (fd %d) is unusable", channel_to_string(i), (*fd)[i]);
}

static int setup_signalfd(const int ch, const sigset_t blocked) {
    if (sigprocmask(SIG_BLOCK, &blocked, NULL) != 0)
        EXITF("Couldn't adjust signal mask");
    const int fd = signalfd(ch, &blocked, SFD_CLOEXEC);
    if (fd == -1)
        EXITF("Couldn't setup sigfd for %d", ch);
    return fd;
}

static sigset_t adjust_signal_delivery(int* const ch) {
    static const int blocked_signals[] = {
        SIGALRM,
        SIGPWR,
        SIGINT
    };
    sigset_t s;
    sigemptyset(&s);
    for (size_t i = 0; i < elementsof(blocked_signals); i++)
        sigaddset(&s, blocked_signals[i]);
    *ch = setup_signalfd(*ch, s);
    return s;
}

int main() {
    init_log();
    int fds[CHANNEL_COUNT] = {
        [CHANNEL_NO_PAYMENT] = -1,
        [CHANNEL_NO_STORAGE] = -1,
        [CHANNEL_NO_UI] = -1,
        [CHANNEL_CO_MASTER] = -1,
        [CHANNEL_FO_MAIN] = STDOUT_FILENO,
        [CHANNEL_II_SIGNAL] = -1,
        [CHANNEL_CI_MASTER] = -1,
        [CHANNEL_NI_DEVICE] = -1,
        [CHANNEL_FI_MAIN] = STDIN_FILENO,
    };
    ucsp_info_and_adjust_fds(&fds[CHANNEL_FI_MAIN], &fds[CHANNEL_FO_MAIN]);
    test_all_channels(&fds);
    sigset_t sigfdset = adjust_signal_delivery(&fds[CHANNEL_II_SIGNAL]);
    struct sus_registation_form tasks[] = {
        sus_registration(fsm_wireformat, fds[CHANNEL_FI_MAIN]),
        sus_registration(fsm_frontend_foreign),
        sus_registration(sus_ioloop, .timeout = 3)
    };
#   if 0
    if (write(3, "\n", 1) != 1 || close(3) != 0)
        EXITF("Readiness notification failed");
#   endif
    if (sus_runall(lengthof(tasks), &tasks) != 0)
        EXITF("Can't start");
    int ret = 0;
    for (size_t i = 0; i < lengthof(tasks); i++) {
        LOGDX("task %zu returned % 2d (%s)", i, tasks[i].result, tasks[i].name);
        if (tasks[i].result)
            ret = 100;
    }
    return ret;
}
