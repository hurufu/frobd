#include "frob.h"
#include "utils.h"
#include <errno.h>
#include <stdlib.h>

#define STRTOL(Src) strtol(Src, NULL, 10)
#define STRBOOL(Src) ((Src)[0] == '1' ? 1 : 0)

%%{
    machine common;
    include frob_common "common.rl";
    variable p *pp;
}%%

static int extract_t1(const byte_t** const pp, const byte_t* const pe, struct frob_t1* const out) {
    int cs;
    %%{
        machine frob_t1;
        include common;

        main := fs;

        write data;
        write init;
        write exec;
    }%%
    return 0;
}

static int extract_t2(const byte_t** const pp, const byte_t* const pe, struct frob_t2* const out) {
    int cs;
    const byte_t* c;
    %%{
        machine frob_t2;
        include common;

        action C {
            c = fpc;
        }
        action Version {
            COPY(out->max_supported_version, c, fpc);
        }
        action Vendor {
            COPY(out->vendor, c, fpc);
        }
        action Type {
            COPY(out->device_type, c, fpc);
        }
        action Id {
            COPY(out->device_id, c, fpc);
        }

        version = a+ >C fs @Version;
        vendor = (a+ >C fs @Vendor) | fs;
        type = (a+ >C fs @Type) | fs;
        id = (a+ >C fs @Id) | fs;

        main := (version vendor type id) |
                (version vendor type) |
                (version vendor) |
                (version);

        write data;
        write init;
        write exec;
    }%%
    return 0;
}

static int extract_t3(const byte_t** const pp, const byte_t* const pe, struct frob_t3* const out) {
    int cs;
    const byte_t* c;

    %%{
        machine frob_t3;
        include common;

        main := fs;

        write data;
        write init;
        write exec;
    }%%

    return cs < %%{ write first_final; }%%;
}

static int extract_t4(const byte_t** const pp, const byte_t* const pe, struct frob_t4* const out) {
    int cs, i = 0;
    const byte_t* c;

    %%{
        machine frob_t4;
        include common;

        action C {
            c = fpc;
        }
        action VersionPP {
            COPY(out->supported_versions[i++], c, fpc);
        }

        version = a+ >C fs @VersionPP;

        main := version*;

        write data;
        write init;
        write exec;
    }%%

    //return cs < %%{ write first_final; }%% ? ENOTRECOVERABLE : 0;
    return 0;
}

static int extract_t5(const byte_t** const pp, const byte_t* const pe, struct frob_t5* const out) {
    int cs;
    const byte_t* c;

    %%{
        machine frob_t5;
        include common;

        action C {
            c = fpc;
        }
        action Version {
            COPY(out->selected_version, c, fpc);
        }

        version = a+ >C fs @Version;

        main := version;

        write data;
        write init;
        write exec;
    }%%

    return cs < %%{ write first_final; }%%;
}

static int extract_s1(const byte_t** const pp, const byte_t* const pe, struct frob_s1* const out) {
    int cs;
    const byte_t* c;

    %%{
        machine frob_s1;
        include common;

        action C {
            c = fpc;
        }
        action TransactionType {
            out->transaction_type = (c[0] == 'S') ? FROB_TRANSACTION_TYPE_PAYMENT : FROB_TRANSACTION_TYPE_VOID;
        }
        action EcrId {
            COPY(out->ecr_id, c, fpc);
        }
        action PaymentId {
            COPY(out->payment_id, c, fpc);
        }
        action AmountGross {
            COPY(out->amount_gross, c, fpc);
        }
        action AmountNet {
            COPY(out->amount_net, c, fpc);
        }
        action Vat {
            COPY(out->vat, c, fpc);
        }
        action Cashback {
            COPY(out->cashback, c, fpc);
        }
        action Currency {
            COPY(out->currency, c, fpc);
        }
        action MaxCashback {
            COPY(out->max_cashback, c, fpc);
        }

        amount = digit+;
        transaction_type = ('S' | 'C') >C fs @TransactionType;
        ecr_id = a+ >C fs @EcrId;
        payment_id = a+ >C fs @PaymentId;
        amount_gross = amount >C fs @AmountGross;
        amount_net = amount >C fs @AmountNet;
        vat = amount >C fs @Vat;
        cashback = amount >C fs @Cashback;
        currency = a{3} >C fs @Currency;
        max_cashback = amount >C fs @MaxCashback;

        required = transaction_type ecr_id payment_id amount_gross amount_net vat currency cashback;

        main := (required) |
                (required max_cashback);

        write data;
        write init;
        write exec;
    }%%

    return cs < %%{ write first_final; }%%;
}

static int extract_d4(const byte_t** const pp, const byte_t* const pe, struct frob_d4* const out) {
    int cs;
    const byte_t* c;

    %%{
        machine frob_d4;
        include common;

        main := fs;

        write data;
        write init;
        write exec;
    }%%

    return 0;
    // FIXME: Use proper FSM
    // return cs < %%{ write first_final; }%% ? ENOTRECOVERABLE : 0;
}

static int extract_d5(const byte_t** const pp, const byte_t* const pe, struct frob_d5* const out) {
    int cs;
    const byte_t* c;

    %%{
        machine frob_d5;
        include common;

        action C {
            c = fpc;
        }

        action DisplayLc {
            out->display_lc = STRTOL(c);
        }
        action DisplayCpl {
            out->display_cpl = STRTOL(c);
        }
        action PrinterCpl {
            out->printer_cpl = STRTOL(c);
        }
        action PrinterCpl2x {
            out->printer_cpl2x = STRTOL(c);
        }
        action PrinterCpl4x {
            out->printer_cpl4x = STRTOL(c);
        }
        action PrinterCpln {
            out->printer_cpln = STRTOL(c);
        }
        action PrinterH2 {
            out->printer_h2 = STRBOOL(c);
        }
        action PrinterH4 {
            out->printer_h4 = STRBOOL(c);
        }
        action PrinterInv {
            out->printer_inv = STRBOOL(c);
        }
        action PrinterQrLength {
            out->printer_max_qrcode_length = STRTOL(c);
        }
        action PrinterBarLength {
            out->printer_max_barcode_length = STRTOL(c);
        }
        action PrinterMaxBitmapCount {
            out->printer_max_bitmap_count = STRTOL(c);
        }
        action PrinterMaxBitmapWidth {
            out->printer_max_bitmap_width = STRTOL(c);
        }
        action PrinterMaxBitmapHeight {
            out->printer_max_bitmap_height = STRTOL(c);
        }
        action PrinterAspectRatio {
            out->printer_aspect_ratio = STRTOL(c);
        }
        action PrinterBufferMaxLines {
            out->printer_buffer_max_lines = STRTOL(c);
        }
        action Enter {
            COPY(out->key_name.enter, c, fpc);
        }
        action Check {
            COPY(out->key_name.check, c, fpc);
        }
        action Cancel {
            COPY(out->key_name.cancel, c, fpc);
        }
        action Backspace {
            COPY(out->key_name.backspace, c, fpc);
        }
        action Delete {
            COPY(out->key_name.delete, c, fpc);
        }
        action Up {
            COPY(out->key_name.up, c, fpc);
        }
        action Down {
            COPY(out->key_name.down, c, fpc);
        }
        action Left {
            COPY(out->key_name.left, c, fpc);
        }
        action Right {
            COPY(out->key_name.right, c, fpc);
        }
        action Nfc {
            out->nfc = STRBOOL(c);
        }
        action Ccr {
            out->ccr = STRBOOL(c);
        }
        action Mcr {
            out->mcr = STRBOOL(c);
        }
        action Bar {
            out->bar = STRBOOL(c);
        }

        display_lc = n* >C fs @DisplayLc;
        display_cpl = n* >C fs @DisplayCpl;
        printer_cpl = n* >C fs @PrinterCpl;
        printer_cpl2x = n* >C fs @PrinterCpl2x;
        printer_cpl4x = n* >C fs @PrinterCpl4x;
        printer_cpln = n* >C fs @PrinterCpln;
        printer_h2 = n >C fs @PrinterH2;
        printer_h4 = n >C fs @PrinterH4;
        printer_inv = n >C fs @PrinterInv;
        printer_max_qrcode_length = n* >C fs @PrinterQrLength;
        printer_max_barcode_length = n* >C fs @PrinterBarLength;
        printer_max_bitmap_count = n* >C fs @PrinterMaxBitmapCount;
        printer_max_bitmap_width = n* >C fs @PrinterMaxBitmapWidth;
        printer_max_bitmap_height = n* >C fs @PrinterMaxBitmapHeight;
        printer_aspect_ratio = n* >C fs @PrinterAspectRatio;
        printer_buffer_max_lines = n* >C fs @PrinterBufferMaxLines;

        enter = a* >C us @Enter;
        cancel = a* >C us @Cancel;
        check = a* >C us @Check;
        backspace = a* >C us @Backspace;
        delete = a* >C us @Delete;
        up = a* >C us @Up;
        down = a* >C us @Down;
        left = a* >C us @Left;
        right = a* >C us @Right;
        key_names = enter cancel check backspace delete up down left right fs;

        nfc = n >C fs @Nfc;
        ccr = n >C fs @Ccr;
        mcr = n >C fs @Mcr;
        bar = n >C fs @Bar;

        required = printer_cpl printer_cpl2x printer_cpl4x printer_cpln printer_h2 printer_h4
                   printer_inv printer_max_barcode_length printer_max_qrcode_length
                   printer_max_bitmap_count printer_max_bitmap_width
                   printer_max_bitmap_height printer_aspect_ratio printer_buffer_max_lines
                   display_lc display_cpl
                   key_names
                   nfc ccr mcr bar
        ;

        main := required;

        write data;
        write init;
        write exec;
    }%%

    return cs < %%{ write first_final; }%% ? ENOTRECOVERABLE : 0;
}

int frob_body_extract(const enum FrobMessageType t,
        const byte_t** const p, const byte_t* const pe,
        union frob_body* const out) {
    switch (t) {
        case FROB_T1: return extract_t1(p, pe, &out->t1);
        case FROB_T2: return extract_t2(p, pe, &out->t2);
        case FROB_T3: return extract_t3(p, pe, &out->t3);
        case FROB_T4: return extract_t4(p, pe, &out->t4);
        case FROB_T5: return extract_t5(p, pe, &out->t5);
        case FROB_S1: return extract_s1(p, pe, &out->s1);
        case FROB_D4: return extract_d4(p, pe, &out->d4);
        case FROB_D5: return extract_d5(p, pe, &out->d5);
        default:
            return ENOSYS;
    }
}
