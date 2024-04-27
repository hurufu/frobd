#include "frob.h"
#include "utils.h"
#include "log.h"
#include <stdlib.h>

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

    LOGIX("UCSPI-%s compatible environment detected (%s)", proto, (connnum ? "server" : "client"));
    char* p = buf;
    for (size_t j = 0; j < lengthof(rl); j++) {
        if (!(ed[j][0] || ed[j][1] || ed[j][2] || ed[j][3]))
            continue;
        p += snprintfx(p, buf + sizeof buf - p, "%s:", rl[j]);
        if (j == 0 && connnum)
            p += snprintfx(p, buf + sizeof buf - p, " #%s", connnum);
        if (ed[j][0])
            p += snprintfx(p, buf + sizeof buf - p, " \"%s\"", ed[j][0]);
        if (ed[j][1] && ed[j][2])
            p += snprintfx(p, buf + sizeof buf - p, " (%s:%s)", ed[j][1], ed[j][2]);
        if (ed[j][3])
            p += snprintfx(p, buf + sizeof buf - p, " [%s]", ed[j][3]);
        LOGDX("%s", buf);
        p = buf;
    }
    if (sv[0]) {
        p = buf;
        p += snprintfx(p, buf + sizeof buf - p, "%s: %s", sv[0], sv[1]);
        if (sv[2])
            snprintfx(p, buf + sizeof buf - p, " \"%s\"", sv[2]);
        LOGDX("%s", buf);
        p = buf;
        if (!connnum) {
            p += snprintfx(p, buf + sizeof buf - p, "HASH:%s SUBJ: %s", sv[3], sv[4]);
            LOGDX("%s", buf);
        }
    }
}

static const char* ucspi_adjust(const char* const proto, int* restrict const in, int* restrict const out) {
    char tmp[16];
    const char* const connnum = getenvfx(tmp, sizeof tmp, "%sCONNNUM", proto);
    // Tested only with s6-networking
    if (!connnum)
        *in = (*out = 6) + 1;
    return connnum;
}

void ucsp_info_and_adjust_fds(int* restrict const in, int* restrict const out) {
    const char* const proto = getenv("PROTO");
    if (proto)
        ucspi_log(proto, ucspi_adjust(proto, in, out));
}
