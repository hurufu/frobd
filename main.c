#include "frob.h"
#include <poll.h>
#include <unistd.h>

#define elementsof(Arr) (sizeof(Arr)/sizeof((Arr)[0]))

int main() {
    struct pollfd pf[] = {
        { .fd = STDIN_FILENO, .events = POLLRDNORM }
    };
    int poll_res;
    int cs;
    while ((poll_res = poll(pf, elementsof(pf), 5*1000)) > 0) {
        for (size_t i = 0; i < elementsof(pf); i++) {
            if (pf[i].revents & POLLRDNORM) {
                unsigned char buf[255];
                cs = frob_main(cs, read(pf[i].fd, buf, sizeof buf), buf);
            }
        }
    }
    switch (poll_res) {
        case 0:
            return 1;
        case -1:
            return 2;
    }
    return 0;
}
