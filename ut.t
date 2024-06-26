#include "frob.h"
#include "utils.h"
#include "log.h"
#include "contextring.h"
#include "utils.h"

#ifdef CK_MAX_ASSERT_MEM_PRINT_SIZE
#   undef CK_MAX_ASSERT_MEM_PRINT_SIZE
#endif
#define CK_MAX_ASSERT_MEM_PRINT_SIZE 1000

#include <errno.h>
#include <unistd.h>
#include <ctype.h>
#include <limits.h>
#include <fcntl.h>
#include <check.h>

// We aren't testing actual I/O here, those functions are used only in the test
// fixtures and support code, so we can just abort on error.
#define XSYSCALL(Type, Func, ...) ({\
    const Type ret = Func(__VA_ARGS__);\
    if (ret == -1)\
        ck_abort_msg(#Func "(" #__VA_ARGS__ ") failed: %m");\
    ret;\
})
#define xpipe(...) XSYSCALL(int, pipe, ##__VA_ARGS__)
#define xclose(...) XSYSCALL(int, close, ##__VA_ARGS__)
#define xwrite(...) XSYSCALL(ssize_t, write, ##__VA_ARGS__)
#define xopen(...) XSYSCALL(int, open, ##__VA_ARGS__)

// TODO: Can this be done during compilation time?
#define FILL_DUMMY_DATA(Buf) do {\
    for (size_t i = 0; i < elementsof(Buf); i++) \
        (Buf)[i] = (i % 0x5F) + 0x20;\
} while (0)

#define OFFSET_OR_PTR(Start, Value)\
    _Generic((Value),\
        int: (Start) + (intptr_t)(Value),\
        size_t: (Start) + (intptr_t)(Value),\
        void*: (Value)\
    )

static int write_data_to_pipe(const size_t l, const char data[static const l]) {
    int pfd[2];
    xpipe(pfd);
    xwrite(pfd[1], data, l);
    xclose(pfd[1]);
    return pfd[0];
}

#define TEST_HEADER1(Input, ExpectedRet)\
    TEST_HEADER3(Input, ExpectedRet, 0, "")
#define TEST_HEADER3(Input, ExpectedRet, ExpectedType, ExpectedToken)\
    test_header(elementsof(Input), (byte_t*)Input, ExpectedRet, ExpectedType, strtoul(ExpectedToken, NULL, 16))
static void test_header(const size_t bs, const byte_t buf[static bs],
                        const int expected_return_value,
                        const enum FrobMessageType expected_type,
                        const token_t expected_token) {
    struct frob_header hdr = {};
    const int ret = frob_header_extract(&buf, buf + bs - 1, &hdr);
    ck_assert_int_eq(ret, expected_return_value);
    ck_assert_int_eq(hdr.type, expected_type);
    ck_assert_uint_eq(hdr.token, expected_token);
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

#define TEST_EREAD(Data, BufSize, ExpectedRet)\
    test_eread(sizeof Data, Data, BufSize, ExpectedRet)
#define TEST_EREAD_OK(Data, BufSize)\
    TEST_EREAD(Data, BufSize, sizeof Data)
static void test_eread(const size_t data_size, const char data[static const data_size], const size_t buf_size, const ssize_t expected_ret) {
    const int fd = write_data_to_pipe(data_size, data);
    input_t out[buf_size];
    const ssize_t r = eread(fd, sizeof out, out);
    ck_assert_int_eq(r, expected_ret);
    ck_assert_mem_eq(out, data, min(data_size, buf_size));
    if (data_size >= buf_size)
        ck_assert_int_eq(errno, EFBIG);
    xclose(fd);
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
    TEST_HEADER3("000102" FS "T1" FS, 0, FROB_T1, "000102");

#test empty_token_is_rejected
    TEST_HEADER1("" FS "T1" FS, EBADMSG);

#test incomplete_header_is_rejected
    TEST_HEADER1("" FS "T1", EBADMSG);

#test short_token_is_padded
    TEST_HEADER3("AA" FS "M1" FS, 0, FROB_M1, "AA");

#test incomplete_hex_is_not_padded
    TEST_HEADER3("B" FS "S2" FS, 0, FROB_S2, "B");

#test incomplete_message_type_is_rejected
    TEST_HEADER1("" FS "T" FS, EBADMSG);

#test empty_message_type_is_rejected
    TEST_HEADER1("ABCDEF" FS "" FS, EBADMSG);

#test complete_but_non_existent_message_type_is_rejected
    TEST_HEADER1("123456" FS "T7" FS, EBADMSG);

#suite body_parsing

#test empty_body_is_handled
    byte_t buf[0];
    const byte_t* p = buf;
    union frob_body body = {}, empty = {};
    const int ret = frob_body_extract(FROB_T3, &p, p, &body);
    ck_assert_int_eq(ret, 0);
    ck_assert_ptr_eq(p, buf);
    ck_assert_mem_eq(&body, &empty, sizeof empty);

#suite utils

#test space_trimming_works_with_only_trailing_spaces
    char buf[] = "hello    ";
    ck_assert_str_eq(trim_whitespaces(buf), "hello");

#test space_trimming_works_with_only_leading_spaces
    char buf[] = "    hello";
    ck_assert_str_eq(trim_whitespaces(buf), "hello");

#test space_trimming_works
    char buf[] = "    hello    ";
    ck_assert_str_eq(trim_whitespaces(buf), "hello");

#test space_trimming_works_with_empty_string
    char buf[] = "";
    ck_assert_str_eq(trim_whitespaces(buf), "");

#test space_trimming_works_with_only_spaces
    char buf[] = "    ";
    ck_assert_str_eq(trim_whitespaces(buf), "");

#test space_trimming_works_with_no_spaces
    char buf[] = "hello";
    ck_assert_str_eq(trim_whitespaces(buf), "hello");

#test space_trimming_spaces_inside_the_string
    char buf[] = "    hello    world    ";
    ck_assert_str_eq(trim_whitespaces(buf), "hello    world");

#test hex_to_nibble_works_inside_the_range
    static const char hex[] = "0123456789abcdef";
    for (byte_t i = 0x0; i <= 0xF; i++) {
        ck_assert_int_eq(hex2nibble(hex[i]), i);
        ck_assert_int_eq(hex2nibble(toupper(hex[i])), i);
    }

#test hex_to_nibble_handles_values_outside_the_range
    for (char i = CHAR_MIN; i < CHAR_MAX; i++)
        switch (i) {
            case 'A' ... 'F':
            case 'a' ... 'f':
            case '0' ... '9':
                continue;
            default:
                ck_assert_int_eq(hex2nibble(i), 0);
                break;
        }

#test eread_file
    TEST_EREAD_OK("Hello, this is some test data", 64);

#test eread_empty_file
    TEST_EREAD_OK(((char[0]){}), 64);

#test eread_empty_file_into_empty_buffer
    TEST_EREAD(((char[0]){}), 0, -1);

#test eread_big_file
    char buf[64 * 1024];
    FILL_DUMMY_DATA(buf);
    TEST_EREAD_OK(buf, sizeof buf + 1);

#test eread_file_into_buffer_with_the_same_size_as_the_file
    char buf[64 * 1024];
    FILL_DUMMY_DATA(buf);
    TEST_EREAD(buf, sizeof buf, -1);

#test eread_file_works_with_empty_buffer
    TEST_EREAD("Hello, this is some test data", 0, -1);

#test eread_file_works_with_small_buffer
    TEST_EREAD("Hello, this is some test data", 1, -1);

#test eread_handles_bad_fd
    input_t out[0];
    const ssize_t r = eread(-1, sizeof out, out);
    ck_assert_int_eq(r, -1);
    ck_assert_int_eq(errno, EBADF);

#test eread_handles_being_interrupted_by_signal
    /* This test case exists to kill specific mutant that was found by Mull.
     * eread retries underlying read system call if it was interrupted by a
     * signal, but that code path is hard to test, so here we fake it by setting
     * errno to EINTR and then checking that it is not overwritten by eread.
     * NOTE: Written data should be 0, because of how mutant are created, Mull
     *       just replaces `read(...) < 0` with `read(...) <= 0`.
     */
    const int fd = write_data_to_pipe(0, "");
    input_t out[5];
    errno = EINTR;
    const ssize_t r = eread(fd, sizeof out, out);
    ck_assert_int_eq(r, 0);
    ck_assert_int_eq(errno, EINTR);
    xclose(fd);

#test snprint_hex_works_for_large_enough_buffer
    static const byte_t data[] = "Hello, this is some test data";
    static const char hex[] = "48656C6C6F2C207468697320697320736F6D652074657374206461746100";
    for (size_t i = sizeof data * 2 + 1; i < sizeof data * 2 + 10; i++) {
        input_t buf[i];
        const int ret = snprint_hex(sizeof buf, buf, sizeof data, data);
        ck_assert_int_eq(ret, sizeof data * 2);
        ck_assert_str_eq((char*)buf, hex);
    }

#test snprint_hex_handles_small_buffer
    static const byte_t data[] = "Hello, this is some test data";
    static const input_t empty[sizeof data * 2 + 1] = "";
    for (size_t i = 0; i < sizeof data * 2 + 1; i++) {
        input_t buf[i];
        memset(buf, 0, sizeof buf);
        const int ret = snprint_hex(sizeof buf, buf, sizeof data, data);
        ck_assert_int_eq(ret, sizeof data * 2);
        ck_assert_int_ge(ret, i);
        ck_assert_mem_eq(buf, empty, sizeof buf);
    }

#test snprint_hex_works_with_empty_data
    static byte_t data[0];
    for (size_t i = 0; i < 5; i++) {
        input_t buf[i];
        const int ret = snprint_hex(sizeof buf, buf, 0, data);
        ck_assert_int_eq(ret, 0);
    }

#test xsnprintf_hex_works_with_empty_data
    static byte_t data[0];
    for (size_t i = 0; i < 5; i++) {
        input_t buf[i];
        const int ret = xsnprint_hex(sizeof buf, buf, 0, data);
        ck_assert_int_eq(ret, 0);
    }

#suite serializing

enum TestMessages { T1, T4, D5, S1 };
static const struct serialization_test_data {
    struct frob_msg msg;
    const char* expected;
} s_test_data[] = {
    [T1] = {
        .msg = {
            .magic = FROB_MAGIC,
            .header = {
                .type = FROB_T1,
                .token = 0x12
            }
        },
        .expected = STX "12" FS "T1" FS ETX "e"
    },
    [T4] = {
        .msg = {
            .magic = FROB_MAGIC,
            .header = {
                .type = FROB_T4,
                .token = 0x0
            },
            .body.t4.supported_versions = {
                "0", "5", "171", "255", "5", "6", "7", "8", "9", "10", "11",
                "12", "13", "14", "15", "16", "17", "18", "19", "20"
            }
        },
        .expected = STX "0" FS "T4" FS "0" US "5" US "171" US "255" US "5" US
                    "6" US "7" US "8" US "9" US "10" US "11" US "12" US "13" US
                    "14" US "15" US "16" US "17" US "18" US "19" US "20" US FS
                    ETX "y"
    },
    [D5] = {
        .msg = {
            .magic = FROB_MAGIC,
            .header = {
                .type = FROB_D5,
                .token = 0xAAA
            },
            .body.d5 = {
                .printer_cpl = 42,
                .printer_cpl2x = 42,
                .printer_cpl4x = 42,
                .printer_cpln = 42,
                .printer_h2 = true,
                .printer_h4 = true,
                .printer_inv = true,
                .printer_max_barcode_length = 42,
                .printer_max_qrcode_length = 42,
                .printer_max_bitmap_count = 42,
                .printer_max_bitmap_width = 42,
                .printer_max_bitmap_height = 42,
                .printer_aspect_ratio = 120,
                .printer_buffer_max_lines = 256,
                .display_lc = 4,
                .display_cpl = 20,
                .key_name = {
                    .enter = "O",
                    .cancel = "X",
                    .check = "C",
                    .backspace = "",
                    .delete = "",
                    .up = "^",
                    .down = "v",
                    .left = "<",
                    .right = ">"
                },
                .device_topo = FROB_DEVICE_TYPE_ECR,
                .nfc = true,
                .ccr = true,
                .mcr = true,
                .bar = true
            }
        },
        .expected = STX "AAA" FS "D5" FS "42" FS "42" FS "42" FS "42" FS "1" FS
                    "1" FS "1" FS "42" FS "42" FS "42" FS "42" FS "42" FS "120"
                    FS "256" FS "4" FS "20" FS "O" US "X" US "C" US "" US "" US
                    "^" US "v" US "<" US ">" US FS "0" FS "1" FS "1" FS "1" FS
                    "1" FS ETX "a"
    },
    [S1] = {
        .msg = {
            .magic = FROB_MAGIC,
            .header = {
                .type = FROB_S1,
                .token = 0x123456
            },
            .body.s1 = {
                .transaction_type = FROB_TRANSACTION_TYPE_PAYMENT,
                .ecr_id = "ECR123",
                .payment_id = "PAY123",
                .amount_gross = "5001",
                .amount_net = "4001",
                .vat = "1000",
                .cashback = "0",
                .max_cashback = "0",
                .currency = "EUR"
            }
        },
        .expected = STX "123456" FS "S1" FS "S" FS "ECR123" FS "PAY123" FS
                    "5001" FS "4001" FS "1000" FS "EUR" FS "0" FS "0" FS ETX "w"
    }
};

#test frame_serialized_correctly_for_large_enough_buffer
    for (size_t i = 0; i < elementsof(s_test_data); i++) {
        const struct serialization_test_data* const q = &s_test_data[i];
        const size_t len = strlen(q->expected);
        for (size_t j = len; j < len + 5; j++) {
            input_t buf[j];
            const ssize_t ret = serialize(sizeof buf, buf, &q->msg);
            ck_assert_int_eq(ret, len);
            ck_assert_mem_eq(buf, q->expected, ret);
        }
    }

#test empty_buffer_is_handled
    input_t buf[0];
    for (size_t i = 0; i < elementsof(s_test_data); i++) {
        const struct serialization_test_data* const q = &s_test_data[i];
        const ssize_t ret = serialize(0, buf, &q->msg);
        ck_assert_int_eq(ret, 0);
    }

#test buffer_too_small_is_handled
    input_t buf[4096];
    for (size_t i = 0; i < elementsof(s_test_data); i++) {
        const struct serialization_test_data* const q = &s_test_data[i];
        for (size_t j = 1; j < strlen(q->expected); j++) {
            const ssize_t ret = serialize(j, buf, &q->msg);
            ck_assert_int_lt(ret, 0);
            ck_assert_int_ge(ret, -j);
            ck_assert_mem_eq(buf, q->expected, -ret);
        }
    }

#suite full_parsing

#test parse_full_message
    for (size_t i = 0; i < elementsof(s_test_data); i++) {
        struct frob_msg msg = { .magic = FROB_MAGIC };
        const struct serialization_test_data* const q = &s_test_data[i];
        const input_t* const start = (input_t*)q->expected + 1;
        const input_t* const end = (input_t*)q->expected + strlen(q->expected) - 2;
        const int ret = parse_message(start, end, &msg);
        ck_assert_int_eq(ret, 0);
        // TODO: Verify actual parsed message
        //ck_assert_mem_eq(&msg, &q->msg, sizeof msg);
    }

#test handle_parse_error
    static input_t d[1024] = "";
    memset(d, '0', sizeof d);
    struct frob_msg msg = { .magic = FROB_MAGIC };
    for (size_t i = 0; i < sizeof d; ++i) {
        const int ret = parse_message(d, d + i, &msg);
        ck_assert_int_eq(ret, -1);
    }

#suite context_ring

#test single_element_can_be_added_and_removed
    struct coro_context tmp = {};
    struct coro_context_ring* ring = NULL;
    insert(&ring, &tmp, NULL);
    ck_assert_ptr_nonnull(ring);
    ck_assert_ptr_eq(ring->ctx, &tmp);
    ck_assert_ptr_eq(ring->next, ring);
    ck_assert_ptr_eq(ring->prev, ring);
    shrink(&ring);
    ck_assert_ptr_null(ring);

#test multiple_elements_can_be_added_and_removed
    struct coro_context_ring* ring = NULL;
    const int max = 1000;
    for (int i = 1; i <= max; i++) {
        struct coro_context tmp[i] = {};
        for (size_t i = 0; i < lengthof(tmp); i++)
            insert(&ring, &tmp[i], NULL);
        struct coro_context_ring* p = ring;
        for (size_t i = 0; i < lengthof(tmp); i++) {
            //ck_assert_ptr_eq(p->ctx, &tmp[(i + max) % max]);
            ck_assert_ptr_eq(p, p->next->prev);
            ck_assert_ptr_eq(p, p->prev->next);
        }
        for (size_t i = 0; i < lengthof(tmp); i++)
            shrink(&ring);
        ck_assert_ptr_null(ring);
    }

#suite coroutines

#test coroutine1

#suite file_operations

#test slurping_whole_file_works
    // We need to close all standard file descriptors, to excercise code that
    // checks if fd < 0
    for (int i = 0; i <= 2; i++)
        close(i);
    input_t buf[256];
    const ssize_t ret = slurp("d5.txt", sizeof buf, buf);
    ck_assert_int_gt(ret, 0);

#test slurp_handles_non_existent_files
    input_t buf[256];
    const ssize_t ret = slurp("does_not_exist.txt", sizeof buf, buf);
    ck_assert_int_eq(ret, -1);
    ck_assert_int_eq(errno, ENOENT);

#test slurp_handles_empty_buffer
    input_t buf[0];
    const ssize_t ret = slurp("empty", sizeof buf, buf);
    ck_assert_int_eq(ret, -1);
    ck_assert_int_eq(errno, EFBIG);

#test xslurp_handles_empty_files
    input_t buf[1];
    const ssize_t ret = xslurp("empty", sizeof buf, buf);
    ck_assert_int_eq(ret, 0);

#test-exit(1) xslurp_exits_on_non_existent_files
    input_t buf[1];
    xslurp("does_not_exist.txt", sizeof buf, buf);

#main-pre
    // Create temporary empty file for testing
    const int fd = xopen("empty", O_CREAT | O_TRUNC | O_WRONLY, 0600);
    xclose(fd);

    #ifndef NO_LOGS_ON_STDERR
    g_log_level = -1;
    #endif
    init_log();
