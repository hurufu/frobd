#include "frob.h"
#include "log.h"
#include "utils.h"
#include "multitasking/sus.h"

struct config {
    version_t supported_versions[4];
    struct frob_device_info info;
    struct frob_d5 parameters;
};

static struct frob_msg xparse_message(const size_t size, const input_t buf[static const size]) {
    struct frob_msg ret = { .magic = FROB_MAGIC };
    if (parse_message(buf, buf + size, &ret) != 0)
        EXITFX("parse failed");
    return ret;
}

static struct frob_d5 load_d5_from_file(const char* const name) {
    input_t buf[256];
    return xparse_message(xslurp(name, sizeof buf, buf), buf).body.d5;
}

static union frob_body construct_hardcoded_message_body(const struct config* const cfg, const enum FrobMessageType t) {
    union frob_body ret = {};
    switch (t) {
        case FROB_T1:
        case FROB_T3:
        case FROB_D4:
        case FROB_P1:
        case FROB_A1:
        case FROB_K1:
            break;
        case FROB_T2:
            ret.t2.info = cfg->info;
            break;
        case FROB_B1:
            ret.b1.info = cfg->info;
            break;
        case FROB_B2:
            ret.b2.info = cfg->info;
            break;
        case FROB_T4:
            memcpy(ret.t4.supported_versions, cfg->supported_versions, sizeof cfg->supported_versions);
            break;
        case FROB_T5:
            memcpy(ret.t5.selected_version, cfg->info.version, sizeof cfg->info.version);
            break;
        case FROB_D5:
            ret.d5 = cfg->parameters;
            break;
        default:
            LOGEX("No hardcoded body for %s (%#x)", frob_type_to_string(t), t);
            assert(false);
    }
    return ret;
}

int autoresponder(const struct autoresponder_args* const args) {
    const struct config cfg = {
        .supported_versions = { "160", "170" },
        .info = {
            .version = "170",
            .vendor = "TEST",
            .device_type = "SIM",
            .device_id = "0"
        },
        .parameters = load_d5_from_file(args->d5_path)
    };
    const unsigned char* p;
    ssize_t bytes;
    unsigned char rsp_buf[1024];
    while ((bytes = sus_borrow(args->in, (void**)&p)) >= 0) {
        //LOGDXP(char tmp[4*bytes], "Received %zd bytes: %s", bytes, PRETTY(p, p + bytes, tmp));
        if (bytes > 1) {
            const struct frob_msg msg = xparse_message(bytes - 1, p + 1);
            const struct frob_msg response = {
                .magic = FROB_MAGIC,
                .header = {
                    .token = msg.header.token,
                    .type = msg.header.type + 1
                },
                .body = construct_hardcoded_message_body(&cfg, msg.header.type + 1)
            };
            const ssize_t w = serialize(sizeof rsp_buf, rsp_buf, &response);
            if (w < 0) {
                LOGEX("Message skipped");
            } else {
                sus_write(args->out, rsp_buf, w);
                LOGDXP(char tmp[4*w], "← % 4zd %s", sizeof w, PRETTY(rsp_buf, rsp_buf + w, tmp));
            }
        }
        //LOGDX("Returning buffer");
        sus_return(args->in, p, bytes);
    }
    return -1;
}
