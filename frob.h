#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef unsigned char byte_t;

enum FrobMessageType {
    // TODO: Consider converting to base-36 – downside it will requires 2 bytes
    // Message classes, numbering is arbitrary
    FROB_T = 0x10,
    FROB_D = 0x20,
    FROB_S = 0x30,
    FROB_P = 0x40,
    FROB_I = 0x50,
    FROB_A = 0x60,
    FROB_K = 0x70,
    FROB_M = 0x80,
    FROB_L = 0x90,
    FROB_B = 0xA0,

    // All message types are in the same order as in the documentation
    FROB_T1 = FROB_T | 0x1, // Communication test – query
    FROB_T2 = FROB_T | 0x2, // Communication test – answer
    FROB_T3 = FROB_T | 0x3, // Version negotiation – inquiry
    FROB_T4 = FROB_T | 0x4, // Version negotiation – proposal
    FROB_T5 = FROB_T | 0x5, // Version negotiation – selection
    FROB_D4 = FROB_D | 0x4, // Configuration – request
    FROB_D5 = FROB_D | 0x5, // Configuration – response
    FROB_S1 = FROB_S | 0x1, // Transaction – initiate
    FROB_S2 = FROB_S | 0x2, // Transaction – complete
    FROB_P1 = FROB_P | 0x1, // Transaction – abort
    FROB_I1 = FROB_I | 0x1, // Transaction – info
    FROB_A1 = FROB_A | 0x1, // Start application – request
    FROB_A2 = FROB_A | 0x2, // Start application – acknowledge
    FROB_D0 = FROB_D | 0x0, // Print – response
    FROB_D1 = FROB_D | 0x1, // Print status – inquiry
    FROB_D6 = FROB_D | 0x6, // Print – command
    FROB_D2 = FROB_D | 0x2, // Print – claim
    FROB_D3 = FROB_D | 0x3, // Print – stop
    FROB_D7 = FROB_D | 0x7, // Print graphics – request
    FROB_D8 = FROB_D | 0x8, // Print graphics – response
    FROB_D9 = FROB_D | 0x9, // Print graphics – store
    FROB_DA = FROB_D | 0xA, // Print graphics - remove
    FROB_K0 = FROB_K | 0x0, // Attendant interaction – response
    FROB_K1 = FROB_K | 0x1, // Attendant interaction – claim
    FROB_K2 = FROB_K | 0x2, // Attendant interaction – interrupt
    FROB_K3 = FROB_K | 0x3, // Attendant interaction – message
    FROB_K4 = FROB_K | 0x4, // Attendant interaction – prompt
    FROB_K5 = FROB_K | 0x5, // Attendant interaction – menu
    FROB_K6 = FROB_K | 0x6, // Attendant interaction – list
    FROB_K7 = FROB_K | 0x7, // Attendant interaction – input
    FROB_K8 = FROB_K | 0x8, // Attendant interaction – card
    FROB_K9 = FROB_K | 0x9, // Attendant interaction – sound
    FROB_M1 = FROB_M | 0x1, // Event – notification
    FROB_L1 = FROB_L | 0x1, // Unavailability – notification
    FROB_B1 = FROB_B | 0x1, // Key pairing – request
    FROB_B2 = FROB_B | 0x2, // Key pairing – response
    FROB_B3 = FROB_B | 0x3, // Key exchange
    FROB_B4 = FROB_B | 0x4  // Key exchange acknowledge
};

struct frob_header {
    enum FrobMessageType type;
    unsigned char token[3];
};

typedef uint8_t bcd_t;
typedef bcd_t amount_t[12];

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

struct frob_t1 { };

struct frob_t2 {
    char max_supported_version[4];
    char vendor[20];
    char device_type[20];
    char device_id[20];
};

struct frob_t3 { };

struct frob_t4 {
    char supported_versions[20][4];
};

struct frob_t5 {
    char selected_version[4];
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
        FROB_DEVICE_TYPE_ECR,
        FROB_DEVICE_TYPE_EFT_WITH_PINPAD_BUILTIN,
        FROB_DEVICE_TYPE_EFT_WITH_PINPAD_EXTERNAL,
        FROB_DEVICE_TYPE_EFT_WITH_PINPAD_PROGRAMMABLE
    } device_type;
    struct {
        char enter[16];
        char cancel[16];
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
    uint32_t error;
    unsigned char card_token[32]; // WTF is this?
    char mid[20];
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
    uint32_t error;
    char msg[100];
};

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
};

struct frob_msg {
    struct frob_header header;
    union frob_body body;
    char attr[10][16];
};

struct frob_frame_fsm_state {
    unsigned char lrc;
    bool not_first;
    int cs;
    unsigned char* p, *pe;
};

int frob_frame_process(struct frob_frame_fsm_state*);
struct frob_header frob_header_extract(const unsigned char** p, const unsigned char* pe);
int frob_protocol_transition(int*, const enum FrobMessageType);
int frob_body_extract(enum FrobMessageType, const unsigned char** p, const unsigned char* pe, union frob_body*);
int frob_extract_additional_attributes(const byte_t**, const byte_t*, char (* const out)[10][16]);
