#pragma once

#include <stdbool.h>
#include <inttypes.h>
#include <assert.h>
#include <errno.h>

/* ***************************************************************** *
 * WARNING: All char arrays are NOT null-terminated, but null-padded *
 * ***************************************************************** */

// Every message shall specify version of API, using this magic string
#define FROB_MAGIC "FROBCr1"

// uint8_t is better than unsigned char to define a byte, because on some
// platforms (unsigned) char may have more than 8 bit (TI C54xx 16 bit). The
// problem is purely theoretical, because I don't target MCUs, but still...
typedef uint8_t byte_t;
typedef char bcd_t; // FIXME: Use a real BCD type
typedef bcd_t amount_t[12];
// TODO: Consider changing type to uint_least8_t, because in practice I doubt that version will go beyond 255
typedef char version_t[4];
// Important: Use appropriate macro to print token_t
typedef uint_least32_t token_t;
#define PRIXTOKEN PRIXLEAST32

enum FrobMessageType {
    FROB_NONE = -1,

    // Message classes, numbering is arbitrary
    FROB_MESSAGE_CLASS_MASK = 0xf0,
    FROB_T = 0x00,
    FROB_D = 0x10,
    FROB_S = 0x20,
    FROB_P = 0x30,
    FROB_I = 0x40,
    FROB_A = 0x50,
    FROB_K = 0x60,
    FROB_M = 0x70,
    FROB_L = 0x80,
    FROB_B = 0x90,

    // Message destinations
    FROB_MESSAGE_CHANNEL_MASK = 0xf00,
    FROB_LOCAL   = 0x100, // L – message handled locally
    FROB_PAYMENT = 0x200, // F – message is forwarded to payment channel
    FROB_STORAGE = 0x300, //     S1 with type C message forwarded to transaction_store channel
    FROB_MAPPED  = 0x400, // M – message is mapped (TBD)
    FROB_UI      = 0x500, // U – message is forwarded to GUI channel

    // All message types are in the same order as in the documentation
    FROB_MESSAGE_NUMBER_MASK = 0x0f,
    FROB_T1 = FROB_LOCAL   | FROB_T | 0x1, // Communication test – query
    FROB_T2 = FROB_LOCAL   | FROB_T | 0x2, // Communication test – answer
    FROB_T3 = FROB_LOCAL   | FROB_T | 0x3, // Version negotiation – inquiry
    FROB_T4 = FROB_LOCAL   | FROB_T | 0x4, // Version negotiation – proposal
    FROB_T5 = FROB_LOCAL   | FROB_T | 0x5, // Version negotiation – selection
    FROB_D4 = FROB_LOCAL   | FROB_D | 0x4, // Configuration – request
    FROB_D5 = FROB_LOCAL   | FROB_D | 0x5, // Configuration – response
    FROB_S1 = FROB_PAYMENT | FROB_S | 0x1, // Transaction – initiate
    FROB_S2 = FROB_PAYMENT | FROB_S | 0x2, // Transaction – complete
    FROB_P1 = FROB_PAYMENT | FROB_P | 0x1, // Transaction – abort
    FROB_I1 = FROB_PAYMENT | FROB_I | 0x1, // Transaction – info
    FROB_A1 = FROB_PAYMENT | FROB_A | 0x1, // Start application – request
    FROB_A2 = FROB_PAYMENT | FROB_A | 0x2, // Start application – acknowledge
    FROB_D0 = FROB_LOCAL   | FROB_D | 0x0, // Print – response
    FROB_D1 = FROB_LOCAL   | FROB_D | 0x1, // Print status – inquiry
    FROB_D6 = FROB_MAPPED  | FROB_D | 0x6, // Print – command
    FROB_D2 = FROB_LOCAL   | FROB_D | 0x2, // Print – claim
    FROB_D3 = FROB_LOCAL   | FROB_D | 0x3, // Print – stop
    FROB_D7 = FROB_MAPPED  | FROB_D | 0x7, // Print graphics – request
    FROB_D8 = FROB_MAPPED  | FROB_D | 0x8, // Print graphics – response
    FROB_D9 = FROB_MAPPED  | FROB_D | 0x9, // Print graphics – store
    FROB_DA = FROB_MAPPED  | FROB_D | 0xA, // Print graphics - remove
    FROB_K0 = FROB_UI      | FROB_K | 0x0, // Attendant interaction – response
    FROB_K1 = FROB_UI      | FROB_K | 0x1, // Attendant interaction – claim
    FROB_K2 = FROB_UI      | FROB_K | 0x2, // Attendant interaction – interrupt
    FROB_K3 = FROB_UI      | FROB_K | 0x3, // Attendant interaction – message
    FROB_K4 = FROB_UI      | FROB_K | 0x4, // Attendant interaction – prompt
    FROB_K5 = FROB_UI      | FROB_K | 0x5, // Attendant interaction – menu
    FROB_K6 = FROB_UI      | FROB_K | 0x6, // Attendant interaction – list
    FROB_K7 = FROB_UI      | FROB_K | 0x7, // Attendant interaction – input
    FROB_K8 = FROB_UI      | FROB_K | 0x8, // Attendant interaction – card
    FROB_K9 = FROB_UI      | FROB_K | 0x9, // Attendant interaction – sound
    FROB_M1 = FROB_STORAGE | FROB_M | 0x1, // Event – notification
    FROB_L1 = FROB_LOCAL   | FROB_L | 0x1, // Unavailability – notification
    FROB_B1 = FROB_LOCAL   | FROB_B | 0x1, // Key pairing – request
    FROB_B2 = FROB_LOCAL   | FROB_B | 0x2, // Key pairing – response
    FROB_B3 = FROB_LOCAL   | FROB_B | 0x3, // Key exchange
    FROB_B4 = FROB_LOCAL   | FROB_B | 0x4  // Key exchange acknowledge
};

enum FrobTransactionStatus {
    FROB_WAIT_FOR_CARD = 20,
    FROB_PAN_CHECKING = 30,
    FROB_WAIT_FOR_MID = 40, // WUT!?
    FROB_WAIT_FOR_AUTHORISATION = 50,
    FROB_WAIT_FOR_AMOUNT_ENTRY = 60,
    FROB_WAIT_FOR_CASHBACK_ENTRY = 65,
    FROB_WAIT_FOR_ORIGINAL_AMOUNT = 70,
    FROB_WAIT_FOR_ORIGINAL_AC = 80,
    FROB_WAIT_FOR_PIN = 90,
    FROB_CONNECTING_TO_ACQUIERER = 100,
    FROB_CONNECTING_TO_ACQUIERER_TRY_2 = 101,
    FROB_CONNECTING_TO_ACQUIERER_TRY_3 = 102,
    FROB_VOICE_AUTHORISATION = 110,
    FROB_SIGNATURE_VERIFICATION = 120,
    FROB_CARDHOLDER_IDENTITY_VERIFICATION = 130,
    FROB_RECEIPT_PRINTING_APPROVED = 140,
    FROB_RECEIPT_PRINTING_COPY = 155,
    FROB_RECEIPT_PRINTING_DECLINED = 150,
    FROB_TRANSACTION_ABORTING = 180,
    FROB_RECEIPT_PRINTING_VOID = 190,
    FROB_OTHER = 1000,
};

struct frob_header {
    enum FrobMessageType type;
    token_t token;
};

struct frob_device_info {
    version_t version;
    char vendor[20];
    char device_type[20];
    char device_id[20];
};

struct frob_t1 { };

struct frob_t2 {
    struct frob_device_info info;
};

struct frob_t3 { };

struct frob_t4 {
    version_t supported_versions[20];
};

struct frob_t5 {
    version_t selected_version;
};

struct frob_d4 { };

struct frob_d5 {
    uint8_t display_lc; // Line count
    uint8_t display_cpl;  // Characters per line
    uint8_t printer_cpl;
    uint8_t printer_cpl2x;
    uint8_t printer_cpl4x;
    uint8_t printer_cpln; // Header CPL
    unsigned short printer_max_barcode_length;
    unsigned short printer_max_qrcode_length;  // WTF is QR code length??
    unsigned short printer_max_bitmap_count;
    unsigned short printer_max_bitmap_width;
    unsigned short printer_max_bitmap_height;
    unsigned short printer_aspect_ratio;
    unsigned short printer_buffer_max_lines;
    enum FrobDeviceType {
        FROB_DEVICE_TYPE_ECR = 0,
        FROB_DEVICE_TYPE_EFT_WITH_PINPAD_BUILTIN,
        FROB_DEVICE_TYPE_EFT_WITH_PINPAD_EXTERNAL,
        FROB_DEVICE_TYPE_EFT_WITH_PINPAD_PROGRAMMABLE
    } device_topo;
    struct {
        char enter[16];
        char cancel[16];
        char check[16];
        char backspace[16];
        char delete[16];
        char up[16];
        char down[16];
        char left[16];
        char right[16];
    } key_name;
    struct {
        bool printer_h2  : 1;
        bool printer_h4  : 1;
        bool printer_inv : 1;
        bool nfc : 1;
        bool ccr : 1;
        bool mcr : 1;
        bool bar : 1;
    };
};

struct frob_s1 {
    enum FrobTransactionType {
        FROB_TRANSACTION_TYPE_PAYMENT,
        FROB_TRANSACTION_TYPE_VOID
    } transaction_type;
    char ecr_id[20];
    char payment_id[20];
    amount_t amount_gross;
    amount_t amount_net;
    amount_t vat;
    amount_t cashback;
    amount_t max_cashback;
    char currency[3]; // ISO 4217
};

struct frob_s2 {
    error_t error;
    byte_t card_token[32]; // WTF is this?
    char mid[20]; // or acquirer id? Who knows what they've tried to say...
    char tid[20];
    char trx_id[20];
    amount_t trx_amount;
    amount_t cashback;
    char payment_method[40];
    char msg[80];
};

struct frob_p1 { };

struct frob_i1 {
    enum FrobTransactionStatus status;
    char msg[80];
};

struct frob_a1 { };

struct frob_a2 {
    error_t error;
    char msg[100];
};

struct frob_b1 {
    struct frob_device_info info;
};

struct frob_b2 {
    struct frob_device_info info;
    error_t result;
    byte_t modulus[256];
    byte_t exponent[3];
};

struct frob_b3 {
    byte_t kek[256];
    byte_t kcv[3];
};

struct frob_b4 {
    error_t result;
};

struct frob_k0 {
    error_t result;
    char output[100];
};

struct frob_k1 { };

union frob_body {
    struct frob_t1 t1;
    struct frob_t2 t2;
    struct frob_t3 t3;
    struct frob_t4 t4;
    struct frob_t5 t5;
    struct frob_d4 d4;
    struct frob_d5 d5;
    struct frob_s1 s1;
    struct frob_s2 s2;
    struct frob_i1 i1;
    struct frob_p1 p1;
    struct frob_a1 a1;
    struct frob_a2 a2;
    struct frob_b1 b1;
    struct frob_b2 b2;
    struct frob_b3 b3;
    struct frob_b4 b4;
    struct frob_k0 k0;
    struct frob_k1 k1;
};

struct frob_msg {
    const char magic[8]; // Shall be set to FROB_MAGIC
    struct frob_header header;
    union frob_body body;
    char attr[23][3];
};

static_assert(sizeof (struct frob_msg) % 16 == 0, "Message shall fit into 16-byte blocks, so output of od(1) will line up nicely");

struct fsm_frontend_foreign_args {
    int cs;
};

struct fsm_frontend_internal_args {
    int cs;
};

struct fsm_frontend_timer_args {
    int cs;
};

struct fsm_wireformat_args {
    const int infd;
};

struct autoresponder_args {
    const char* const d5_path; // Temporary!
    int in; // Id of input buffer
    int out; // Output file descriptor
};

struct sighandler_args {
};

struct controller_args {
};

struct s6_notify_args {
    const int fd;
};

int fsm_wireformat(void*);
int fsm_frontend_foreign(struct fsm_frontend_foreign_args*);
int fsm_frontend_internal(struct fsm_frontend_internal_args*);
int fsm_frontend_timer(struct fsm_frontend_timer_args*);
int autoresponder(const struct autoresponder_args*);
int sighandler(struct sighandler_args*);
int controller(struct controller_args*);
int s6_notify(const struct s6_notify_args*);

void ucsp_info_and_adjust_fds(int* restrict in, int* restrict out);
