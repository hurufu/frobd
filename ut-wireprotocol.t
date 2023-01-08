#include "frob.h"
#include "utils.h"
#include <check.h>
#include <errno.h>

#define OFFSET_OR_PTR(Start, Value)\
    _Generic((Value),\
        int: (Start) + (intptr_t)(Value),\
        size_t: (Start) + (intptr_t)(Value),\
        void*: (Value)\
    )

#define TEST_HEADER(Input, ExpectedType, ...)\
    test_header(elementsof(Input), (byte_t*)Input, ExpectedType, &(byte_t[3]){__VA_ARGS__})
static void test_header(const size_t bs, const byte_t buf[static bs],
                        const enum FrobMessageType expected_type,
                        const byte_t (*expected_token)[3]) {
    const struct frob_header hdr = frob_header_extract(&buf, buf + bs - 1);
    ck_assert_int_eq(hdr.type, expected_type);
    ck_assert_mem_eq(hdr.token, *expected_token, elementsof(*expected_token));
}

#define TEST_FRAME(Input, ExpectedRet, ExpectedP, ExpectedPe) do {\
    byte_t buf[] = Input;\
    test_frame(elementsof(buf), buf, ExpectedRet, OFFSET_OR_PTR(buf,ExpectedP), OFFSET_OR_PTR(lastof(buf),ExpectedPe));\
} while (0)
static void test_frame(const size_t bs, byte_t buf[static bs],
                       const int expected_return_value,
                       const byte_t* const expected_p, const byte_t* const expected_pe) {
    struct frob_frame_fsm_state st = {.p = buf, .pe = buf + bs};
    const int x = frob_frame_process(&st);
    ck_assert_int_eq(x, expected_return_value);
    ck_assert_ptr_eq(st.p, expected_p);
    ck_assert_ptr_eq(st.pe, expected_pe);
}

#suite frame_parsing

#test simple_frame_parsed_correctly
    TEST_FRAME(STX "000102" FS "T1" FS ETX "e", 0, 1, -2);

#test frame_with_invalid_lrc_produces_error
    TEST_FRAME(STX "000102" FS "T1" FS ETX "&", EBADMSG, 1, -2);

#test empty_frame_is_handled_gracefully
    byte_t buf[0];
    struct frob_frame_fsm_state st = {.p = buf, .pe = buf};
    const int x = frob_frame_process(&st);
    ck_assert_int_eq(x, EAGAIN);
    ck_assert_ptr_eq(st.p, buf);
    ck_assert_ptr_null(st.pe);

#test incomplete_frame_returns_eagain
    TEST_FRAME(STX "000102", EAGAIN, 1, NULL);

#test any_junk_before_correct_frame_is_skipped
    TEST_FRAME("gar\0bage" STX "23AB" FS "T2" FS "170" FS "COMPANY" FS "SAMPLE MESSAGE" FS "02" FS ETX "y", 0, sizeof("gar\0bage"), -2);

#test incomplete_frame_can_be_resumed
    byte_t buf1[] = STX "000102" FS;
    struct frob_frame_fsm_state st = {.p = buf1, .pe = lastof(buf1)};
    int x = frob_frame_process(&st);
    ck_assert_int_eq(x, EAGAIN);
    ck_assert_ptr_eq(st.p, buf1 + 1);
    ck_assert_ptr_null(st.pe);

    byte_t buf2[] = "T1" FS;
    st.p = buf2;
    st.pe = lastof(buf2);
    x = frob_frame_process(&st);
    ck_assert_int_eq(x, EAGAIN);
    ck_assert_ptr_null(st.pe);
    ck_assert_ptr_eq(st.p, buf2);

    byte_t buf3[] = ETX "e";
    st.p = buf3;
    st.pe = lastof(buf3);
    x = frob_frame_process(&st);
    ck_assert_int_eq(x, 0);
    ck_assert_ptr_eq(st.p, buf3);
    ck_assert_ptr_eq(st.pe, buf3);

#suite header_parsing

#test simple_header_parsed_correctly
    TEST_HEADER("000102" FS "T1" FS, FROB_T1, 0x00, 0x01, 0x02);

#test empty_token_is_rejected
    TEST_HEADER("" FS "T1" FS, 0);

#test incomplete_header_is_rejected
    TEST_HEADER("" FS "T1", 0);

#test short_token_is_padded
    TEST_HEADER("AA" FS "M1" FS, FROB_M1, 0xAA);

#test incomplete_hex_is_padded
    TEST_HEADER("B" FS "S2" FS, FROB_S2, 0xB0);

#test incomplete_message_type_is_rejected
    TEST_HEADER("" FS "T" FS, 0);

#test empty_message_type_is_rejected
    TEST_HEADER("ABCDEF" FS "" FS, 0);

#test complete_but_non_existent_message_type_is_rejected
    TEST_HEADER("123456" FS "T7" FS, 0);
