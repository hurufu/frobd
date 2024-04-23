#include "frob.h"
#include "log.h"
#include "utils.h"
#include "multitasking/sus.h"

int autoresponder(void*) {
    ssize_t bytes;
    const char* p;
    while ((bytes = sus_borrow(1, (void**)&p)) >= 0) {
        unsigned char t2[] = "1T2T";
        sus_write(6, t2, sizeof t2);
        LOGDXP(char tmp[4*sizeof t2], "← % 4zd %s", sizeof t2, PRETTY(t2, t2 + sizeof t2, tmp));
        sus_return(1, p, bytes);
    }
    return -1;
}
