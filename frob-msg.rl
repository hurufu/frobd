#include "frob.h"

static struct FrobMsg s_m;
static consumer_t s_consumer;

static int crc(const unsigned char* const p) {
    return *p;
}

static void text(const unsigned char* const p) {
}

static void file(const unsigned char* const p) {
}

static void unit(const unsigned char* const p) {
}

%%{
    machine frob;
    alphtype unsigned char;

    action CRC {
        if (crc(fpc) != 0) {
            fbreak;
        }
    }

    action Text {
        text(fpc);
    }

    action File {
        file(fpc);
    }

    action Unit {
        unit(fpc);
    }

    action Number {
        number(fpc);
    }

    stx = 0x02;
    etx = 0x03;
    us  = 0x1F;
    fs  = 0x1C;
    del = 0x7F;

    n = [0-9];
    a = ^cntrl | del;
    h = xdigit;
    b = [01];
    s = a* us @Unit;

    nf = n* fs @File;
    af = a* fs @File;
    hf = h* fs @File;
    bf = b* fs @File;
    sf = s* fs @File;

    T1 = zlen;
    T2 = af af? af? af?;
    T3 = zlen;
    T4 = sf;

    msg := stx (nf|af|hf|bf|sf)* etx @CRC any @Text;
}%%

%% write data;

ssize_t frob_match(const producer_t producer, const consumer_t consumer) {
    ssize_t s;
    unsigned char buf[2 * 1024];
    int cs;
    %% write init;
    s_consumer = consumer;
    s_m = (struct FrobMsg){ };
    while ((s = producer(&buf)) > 0) {
        const unsigned char* p = buf, * const pe = buf + s;
        %% write exec;
    }
    return s;
}
