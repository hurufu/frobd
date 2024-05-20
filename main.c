#include "utils.h"
#include "multitasking/sus.h"

int main(const int ac, const char* av[static const ac]) {
    if (ac != 3)
        return 1;
    const union iopair foreign = ucsp_info_and_adjust_fds();
    sus_create(fsm_wireformat, foreign.r);
    return sus_main();
}
