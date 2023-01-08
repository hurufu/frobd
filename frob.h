#include <unistd.h>
#include <stdbool.h>

typedef unsigned char byte_t;

enum FrobMessageType {
    // TODO: Consider converting to base-36
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

struct frob_frame_fsm_state {
    unsigned char lrc;
    bool not_first;
    int cs;
    unsigned char* p, *pe;
};

int frob_frame_process(struct frob_frame_fsm_state*);
struct frob_header frob_header_extract(const unsigned char* p, const unsigned char* pe);
int frob_protocol_transition(int*, const enum FrobMessageType);
