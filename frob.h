#include <unistd.h>
#include <stdbool.h>

struct frob_frame_fsm_state {
    unsigned char lrc;
    bool not_first;
    int cs;
    unsigned char* p, *pe;
};

int frob_frame_process(struct frob_frame_fsm_state*);
