#include "frob.h"
#include "utils.h"
#include <check.h>

#suite frame_parsing

#test first
    const byte_t buf[] = "abcdef";
    struct frob_frame_fsm_state st = {.p = buf, .pe = lastof(buf)};
    const int x = frob_frame_process(&st);
    ck_assert_int_eq(x, 0);
