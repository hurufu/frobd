#include <stdint.h>
#include <unistd.h>

enum FrobType {
    FROB_S1 = 0x01,
    FROB_S2 = 0x02,
    FROB_T1 = 0x11,
    FROB_T2 = 0x12
};

enum OperationType {
    FROB_PAYMENT = 'S',
    FROB_STATUS = 'C'
};

struct Frob_S1 {
    enum OperationType operation;
    char ecr_id[20];          char : 0;
    char purchase_id[20];     char : 0;
    char amount_gross[12];    char : 0;
    char amount_net[12];      char : 0;
    char amount_vat[12];      char : 0;
    char currency[3];         char : 0;
    char amount_cashback[12]; char : 0;
    char max_cashback[12];    char : 0;
};

struct Frob_S2 {
    char error_code[6];            char : 0;
    unsigned char card_token[32];
    char mid[20];                  char : 0;
    char tid[20];                  char : 0;
    char trxid[20];                char : 0;
    char amount_payed[12];         char : 0;
    char cashback_to_withdraw[12]; char : 0;
    char payment_method[40];       char : 0;
    char error_message[80];
};

struct Frob_T1 {
};

struct Frob_T2 {
    char version[4];      char : 0;
    char vendor[20];      char : 0;
    char device_type[20]; char : 0;
    char device_id[20];   char : 0;
};

struct FrobMsg {
    unsigned char token[3];          char : 0;
    enum FrobType type;
    union {
        struct Frob_S1 s1;
        struct Frob_S2 s2;
        struct Frob_T1 t1;
        struct Frob_T2 t2;
    };
    char additional_attributes[100]; char : 0;// Delimeted by '\0'
};

typedef ssize_t (*producer_t)(unsigned char (*)[2*1024]);
typedef int (*consumer_t)(const struct FrobMsg*);
typedef int (*acknowledge_t)(unsigned char);

struct FrobInterface {
    size_t input_buffer_size;
    unsigned char* input_buffer;
    producer_t producer;
    acknowledge_t acknowledge;
    consumer_t consumer;
};

ssize_t frob_match(producer_t, consumer_t);
