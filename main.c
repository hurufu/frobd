#include "frob.h"
#include <poll.h>
#include <unistd.h>
#include <stdio.h>

#define elementsof(Arr) (sizeof(Arr)/sizeof((Arr)[0]))

int main() {
    struct pollfd pf[] = {
        { .fd = STDIN_FILENO, .events = POLLRDNORM }
    };
    int poll_res;
    while ((poll_res = poll(pf, elementsof(pf), 12 * 1000)) > 0) {
        if (pf[0].revents & POLLNVAL) {
            break;
        }
        if (pf[0].revents & POLLHUP) {
            break;
        }
        if (pf[0].revents & POLLRDNORM) {
            unsigned char buf[1];
            const ssize_t s = read(pf[0].fd, buf, sizeof buf);
            if (s <= 0) {
                break;
            }
            frob_process_ecr_eft_input(s, buf);
        }
    }
    for (size_t i = 0; i < elementsof(pf); i++) {
        close(pf[i].fd);
    }
    switch (poll_res) {
        case 0:
            return 1;
        case -1:
            return 2;
    }
    return 0;
}
