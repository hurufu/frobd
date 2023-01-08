#include "frob.h"
#include "utils.h"
#include <check.h>
#include <errno.h>

#suite frame_parsing

#test simple_frame_parsed_correctly
    byte_t buf[] = STX "000102" FS "T1" FS ETX "e";
    struct frob_frame_fsm_state st = {.p = buf, .pe = lastof(buf)};
    const int x = frob_frame_process(&st);
    ck_assert_int_eq(x, 0);
    ck_assert_ptr_eq(st.p, buf + 1);
    ck_assert_ptr_eq(st.pe, lastof(buf) - 2);

#test frame_with_invalid_lrc_produces_error
    byte_t buf[] = STX "000102" FS "T1" FS ETX "&";
    struct frob_frame_fsm_state st = {.p = buf, .pe = lastof(buf)};
    const int x = frob_frame_process(&st);
    ck_assert_int_eq(x, EBADMSG);
    ck_assert_ptr_eq(st.p, buf + 1);
    ck_assert_ptr_eq(st.pe, lastof(buf) - 2);

#test empty_frame_is_handled_gracefully
    byte_t buf[0];
    struct frob_frame_fsm_state st = {.p = buf, .pe = buf};
    const int x = frob_frame_process(&st);
    ck_assert_int_eq(x, EAGAIN);
    ck_assert_ptr_eq(st.p, NULL);
    ck_assert_ptr_eq(st.pe, NULL);

#test incomplete_frame_returns_eagain
    byte_t buf[] = STX "000102";
    struct frob_frame_fsm_state st = {.p = buf, .pe = lastof(buf)};
    const int x = frob_frame_process(&st);
    ck_assert_int_eq(x, EAGAIN);
    ck_assert_ptr_eq(st.p, buf + 1);
    ck_assert_ptr_eq(st.pe, NULL);

#test any_junk_before_correct_frame_is_skipped
    byte_t buf[] = "garbage before" STX "23AB" FS "T2" FS "170" FS "COMPANY" FS
                   "SAMPLE MESSAGE" FS "02" FS ETX "y";
    struct frob_frame_fsm_state st = {.p = buf, .pe = lastof(buf)};
    const int x = frob_frame_process(&st);
    ck_assert_int_eq(x, 0);
    ck_assert_ptr_eq(st.p, buf + sizeof ("garbage before"));
    ck_assert_ptr_eq(st.pe, lastof(buf) - 2);

#test incomplete_frame_can_be_resumed
    byte_t buf1[] = STX "000102" FS;
    struct frob_frame_fsm_state st = {.p = buf1, .pe = lastof(buf1)};
    int x = frob_frame_process(&st);
    ck_assert_int_eq(x, EAGAIN);
    ck_assert_ptr_eq(st.p, buf1 + 1);
    ck_assert_ptr_eq(st.pe, NULL);

    byte_t buf2[] = "T1" FS;
    st.p = buf2;
    st.pe = lastof(buf2);
    x = frob_frame_process(&st);
    ck_assert_int_eq(x, EAGAIN);
    ck_assert_ptr_eq(st.pe, NULL);
    ck_assert_ptr_eq(st.p, buf2);

    byte_t buf3[] = ETX "e";
    st.p = buf3;
    st.pe = lastof(buf3);
    x = frob_frame_process(&st);
    ck_assert_int_eq(x, 0);
    ck_assert_ptr_eq(st.p, buf3);
    ck_assert_ptr_eq(st.pe, buf3);
