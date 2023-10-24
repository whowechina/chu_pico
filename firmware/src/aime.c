/*
 * AIME Reader
 * WHowe <github.com/whowechina>
 * 
 * Use PN532 to read AIME
 */

#include <stdint.h>
#include <stdbool.h>

#include "bsp/board.h"
#include "hardware/gpio.h"
#include "hardware/i2c.h"

#include "board_defs.h"

#include "i2c_hub.h"

#include "pn532.h"

#define FRAME_TIMEOUT 200000

enum {
    CMD_GET_FW_VERSION = 0x30,
    CMD_GET_HW_VERSION = 0x32,
    // Card read
    CMD_START_POLLING = 0x40,
    CMD_STOP_POLLING = 0x41,
    CMD_CARD_DETECT = 0x42,
    CMD_CARD_SELECT = 0x43,
    CMD_CARD_HALT = 0x44,
    // MIFARE
    CMD_MIFARE_KEY_SET_A = 0x50,
    CMD_MIFARE_AUTHORIZE_A = 0x51,
    CMD_MIFARE_READ = 0x52,
    CMD_MIFARE_WRITE = 0x53,
    CMD_MIFARE_KEY_SET_B = 0x54,
    CMD_MIFARE_AUTHORIZE_B = 0x55,
    // Boot,update
    CMD_TO_UPDATER_MODE = 0x60,
    CMD_SEND_HEX_DATA = 0x61,
    CMD_TO_NORMAL_MODE = 0x62,
    CMD_SEND_BINDATA_INIT = 0x63,
    CMD_SEND_BINDATA_EXEC = 0x64,
    // FeliCa
    CMD_FELICA_PUSH = 0x70,
    CMD_FELICA_THROUGH = 0x71,
    CMD_FELICA_THROUGH_POLL = 0x00,
    CMD_FELICA_THROUGH_READ = 0x06,
    CMD_FELICA_THROUGH_WRITE = 0x08,
    CMD_FELICA_THROUGH_GET_SYSTEM_CODE = 0x0C,
    CMD_FELICA_THROUGH_NDA_A4 = 0xA4,
    // LED board
    CMD_EXT_BOARD_LED = 0x80,
    CMD_EXT_BOARD_LED_RGB = 0x81,
    CMD_EXT_BOARD_LED_RGB_UNKNOWN = 0x82,  // 未知
    CMD_EXT_BOARD_INFO = 0xf0,
    CMD_EXT_FIRM_SUM = 0xf2,
    CMD_EXT_SEND_HEX_DATA = 0xf3,
    CMD_EXT_TO_BOOT_MODE = 0xf4,
    CMD_EXT_TO_NORMAL_MODE = 0xf5,
};

enum {  // 未确认效果
    STATUS_OK = 0,
    STATUS_NFCRW_INIT_ERROR = 1,
    STATUS_NFCRW_FIRMWARE_UP_TO_DATE = 3,
    STATUS_NFCRW_ACCESS_ERROR = 4,
    STATUS_CARD_DETECT_TIMEOUT = 5,
    STATUS_CARD_DETECT_ERROR = 32,
    STATUS_FELICA_ERROR = 33,
};

const char *fw_version[] = { "TN32MSEC003S F/W Ver1.2", "\x94" };
const char *hw_version[] = { "TN32MSEC003S H/W Ver3.0", "837-15396" };
const char *led_info[] = { "15084\xFF\x10\x00\x12", "000-00000\xFF\x11\x40" };
static int baudrate_mode = 0;

static void aime_set_baudrate(int mode)
{
    baudrate_mode = (mode == 0) ? 0 : 1;
}

static int aime_interface = -1;

void aime_init(int interface)
{
    aime_interface = interface;

    i2c_select(I2C_PORT, 1 << 5); // PN532 on IR1 (I2C mux chn 5)
    pn532_config_sam();
}

union __attribute__((packed)) {
    struct {
        uint8_t len;
        uint8_t addr;
        uint8_t seq;
        uint8_t cmd;
        uint8_t status;
        uint8_t payload_len;
        uint8_t payload[];
    };
    uint8_t raw[256];
} response;

union __attribute__((packed)) {
    struct {
        uint8_t len;
        uint8_t addr;
        uint8_t seq;
        uint8_t cmd;
        uint8_t payload_len;
        uint8_t payload[];
    };
    uint8_t raw[256];
} request;

struct {
    bool active;
    uint8_t len;
    uint8_t check_sum;
    bool escaping;
    uint64_t time;
} req_ctx;

static uint8_t key_sets[2][6]; // 'KeyA' and 'KeyB'

static void build_response(int payload_len)
{
    response.len = payload_len + 6;
    response.addr = request.addr;
    response.seq = request.seq;
    response.cmd = request.cmd;
    response.status = STATUS_OK;
    response.payload_len = payload_len;
}

static void send_response()
{
    uint8_t checksum = 0;

    uint8_t sync = 0xe0;
    tud_cdc_n_write(aime_interface, &sync, 1);

    for (int i = 0; i < response.len; i++) {
        uint8_t c = response.raw[i];
        checksum += c;
        if (c == 0xe0 || c == 0xd0) {
            uint8_t escape = 0xd0;
            tud_cdc_n_write(aime_interface, &escape, 1);
            c--;
        }
        tud_cdc_n_write(aime_interface, &c, 1);
    }

    tud_cdc_n_write(aime_interface, &checksum, 1);
    tud_cdc_n_write_flush(aime_interface);
}

static void simple_response(uint8_t status)
{
    build_response(0);
    response.status = status;
    send_response();
}

static void cmd_to_normal_mode()
{
    simple_response(pn532_firmware_ver() ? STATUS_NFCRW_FIRMWARE_UP_TO_DATE
                                         : STATUS_NFCRW_INIT_ERROR);
}

static void cmd_fake_version(const char *version[])
{
    int len = strlen(version[baudrate_mode]);
    build_response(len);
    memcpy(response.payload, version[baudrate_mode], len);
    send_response();
}

static void cmd_get_hw_version()
{
    build_response(2);
    response.payload[0] = 0x01;
    response.payload[1] = 0x00;
    send_response();
}

static void cmd_key_set(int type)
{
    memcpy(key_sets[type], request.payload, 6);
    build_response(0);
    send_response();
}

static void cmd_set_polling(bool enabled)
{
    pn532_set_rf_field(0, enabled ? 1 : 0);
    simple_response(STATUS_OK);
}

static void cmd_detect_card()
{
    typedef struct __attribute__((packed)) {
        uint8_t count;
        uint8_t type;
        uint8_t id_len;
        uint8_t idm[8];
        uint8_t pmm[8];
        uint8_t syscode[2];
    } card_info_t;
    
    card_info_t *card = (card_info_t *) response.payload;

    int len = sizeof(card->idm);
    if (pn532_poll_mifare(card->idm, &len)) {
        build_response(len > 4 ? 10 : 7);
        card->count = 1;
        card->type = 0x10;
        card->id_len = len;
        printf("Card Mifare %d\n", card->id_len);
    } else if (pn532_poll_felica(card->idm, card->pmm, card->syscode)) {
        build_response(19);
        card->count = 1;
        card->type = 0x20;
        card->id_len = 16;
        printf("Card Felica %d\n", card->id_len);
    } else {
        build_response(1);
        card->count = 0;
        response.status = STATUS_OK;
    }
    send_response();
}

typedef struct __attribute__((packed)) {
    uint8_t idm[8];
    uint8_t len;
    uint8_t code;
    uint8_t data[0];
} felica_encap_req_t;

union req {
    struct {
        uint8_t system_code[2];
        uint8_t request_code;
        uint8_t timeout;
    } poll;
    struct {
        uint8_t rw_idm[8];
        uint8_t service_num;
        uint8_t service_code[2];
        uint8_t block_num;
        uint8_t block_list[0][2];
    } other;
    uint8_t payload[32];
};

typedef struct __attribute__((packed)) {
    uint8_t len;
    uint8_t code;
    uint8_t idm[8];
    uint8_t data[0];
} felica_encap_resp_t;

union resp {

    struct {
        uint8_t rw_status[2];
        uint8_t block_num;
        uint8_t blocks[1][16];
    } other;
    uint8_t payload[32];
};


typedef struct __attribute__((packed)) {
    uint8_t idm[8];
    uint8_t pmm[8];
    uint8_t syscode[2];
} card_id_t;

static int cmd_felica_encap_poll(const card_id_t *card)
{
    typedef struct __attribute__((packed)) {
        uint8_t pmm[8];
        uint8_t syscode[2];
    } encap_poll_resp_t;

    printf("\tPOLL\n");

    felica_encap_resp_t *encap_resp = (felica_encap_resp_t *) response.payload;
    encap_poll_resp_t *poll_resp = (encap_poll_resp_t *) encap_resp->data;

    build_response(20);
    memcpy(poll_resp->pmm, card->pmm, 8);
    memcpy(poll_resp->syscode, card->syscode, 2);

    return sizeof(*poll_resp);
}

static int cmd_felica_encap_get_syscode(const card_id_t *card)
{
    printf("\tGET_SYSTEM_CODE\n");
    felica_encap_resp_t *encap_resp = (felica_encap_resp_t *) response.payload;
    encap_resp->data[0] = 0x01;
    encap_resp->data[1] = card->syscode[0];
    encap_resp->data[2] = card->syscode[1];
    return 3;
}

static int cmd_felica_encap_read()
{
    printf("\tREAD\n");
    felica_encap_req_t *encap_req = (felica_encap_req_t *) request.payload;
    uint8_t *req_data = encap_req->data;

    printf("Felica Encap Request:");
    for (int i = 0; i < request.payload_len; i++) {
        printf(" %02x", request.payload[i]);
    }
    printf("\n");

    uint8_t *service = req_data + 8;
    uint8_t svc_num = service[0];
    uint16_t svc_codes[svc_num];
    for (int i = 0; i < svc_num; i++) {
        svc_codes[i] = service[i + 1] | (service[i + 2] << 8);
    }

    uint8_t *block = req_data + 9 + svc_num * 2;
    uint8_t block_num = block[0];
    uint16_t block_list[block_num];
    for (int i = 0; i < block_num; i++) {
        block_list[i] = (block[i + 1] << 8) | block[i + 2];
    }

    typedef struct __attribute__((packed)) {
        uint8_t rw_status[2];
        uint8_t block_num;
        uint8_t block_data[0][16];
    } encap_read_resp_t;

    felica_encap_resp_t *encap_resp = (felica_encap_resp_t *) response.payload;
    encap_read_resp_t *read_resp = (encap_read_resp_t *) encap_resp->data;

    if (!pn532_felica_read_no_encrypt(svc_num, svc_codes, block_num, block_list,
                                      read_resp->block_data)) {
        printf("Felica READ failed\n");
        return -1;
    }

    read_resp->rw_status[0] = 0x00;
    read_resp->rw_status[1] = 0x00;
    read_resp->block_num = block_num;

    return sizeof(*read_resp);
}

#if 0

        break;

    case CMD_FELICA_THROUGH_NDA_A4:
        printf("\tNDA_A4\n");
        build_response(11);
        resp->payload[0] = 0x00;
        break;
    case CMD_FELICA_THROUGH_WRITE:
        printf("\tWRITE\n");
        build_response(12);
        resp->other.rw_status[0] = 0;
        resp->other.rw_status[1] = 0;
        break;
    default:
        printf("\tUnknown through: %02x\n", code);
        build_response(0);
        response.status = STATUS_OK;
#endif

static void cmd_felica_encap()
{
    felica_encap_req_t *encap_req = (felica_encap_req_t *) request.payload;

    card_id_t card;

    if (!pn532_poll_felica(card.idm, card.pmm, card.syscode)) {
        simple_response(STATUS_FELICA_ERROR);        
    }

    printf("Felica Encap (%02x):", encap_req->code);
    for (int i = 0; i < request.payload_len; i++) {
        printf(" %02x", request.payload[i]);
    }
    printf("\n");

    int datalen = -1;
    switch (encap_req->code) {
        case CMD_FELICA_THROUGH_GET_SYSTEM_CODE:
            datalen = cmd_felica_encap_get_syscode(&card);
            break;
        case CMD_FELICA_THROUGH_POLL:
            datalen = cmd_felica_encap_poll(&card);
            break;
        case CMD_FELICA_THROUGH_READ:
            datalen = cmd_felica_encap_read();
            break;
        default:
            printf("Unknown code %d\n", encap_req->code);
            break;
    }

    if (datalen < 0) {
        simple_response(STATUS_FELICA_ERROR);
        return;
    }

    felica_encap_resp_t *encap_resp = (felica_encap_resp_t *) response.payload;
    build_response(sizeof(*encap_resp) + datalen);
    encap_resp->len = response.payload_len;
    encap_resp->code = encap_req->code + 1;
    memcpy(encap_resp->idm, card.idm, 8);

    send_response();

    printf("Felica Encap Response:");
    for (int i = 0; i < response.payload_len; i++) {
        printf(" %02x", response.payload[i]);
    }
    printf("\n");
}

static uint32_t led_color;

static void cmd_led_rgb()
{
    led_color = request.payload[0] << 16 | request.payload[1] << 8 | request.payload[2];
    build_response(0);
    send_response();
}

static void aime_handle_frame()
{
    i2c_select(I2C_PORT, 1 << 5); // PN532 on IR1 (I2C mux chn 5)

    switch (request.cmd) {
        case CMD_TO_NORMAL_MODE:
            cmd_to_normal_mode();
            break;
        case CMD_GET_FW_VERSION:
            printf("CMD_GET_FW_VERSION\n");
            cmd_fake_version(fw_version);
            break;
        case CMD_GET_HW_VERSION:
            printf("CMD_GET_HW_VERSION\n");
            cmd_fake_version(hw_version);
            break;
        case CMD_MIFARE_KEY_SET_A:
            printf("CMD_MIFARE_KEY_SET_A\n");
            cmd_key_set(0);
            break;
        case CMD_MIFARE_KEY_SET_B:
            printf("CMD_MIFARE_KEY_SET_B\n");
            cmd_key_set(1);
            break;

        case CMD_START_POLLING:
            cmd_set_polling(true);
            break;
        case CMD_STOP_POLLING:
            cmd_set_polling(false);
            break;
        case CMD_CARD_DETECT:
            cmd_detect_card();
            break;
        case CMD_FELICA_THROUGH:
            cmd_felica_encap();
            break;

        case CMD_CARD_SELECT:
        case CMD_MIFARE_AUTHORIZE_B:
        case CMD_MIFARE_READ:
        case CMD_CARD_HALT:
            break;

        case CMD_EXT_BOARD_INFO:
            cmd_fake_version(led_info);
            break;
        case CMD_EXT_BOARD_LED_RGB:
            cmd_led_rgb();
            break;

        case CMD_SEND_HEX_DATA:
        case CMD_EXT_TO_NORMAL_MODE:
            simple_response(STATUS_OK);
            break;

        default:
            printf("Unknown command: %02x [", request.cmd);
            for (int i = 0; i < request.len; i++) {
                printf(" %02x", request.raw[i]);
            }
            printf("]\n");
            simple_response(STATUS_OK);
            break;
    }
}

static bool aime_feed(int c)
{
    if (c == 0xe0) {
        req_ctx.active = true;
        req_ctx.len = 0;
        req_ctx.check_sum = 0;
        req_ctx.escaping = false;
        req_ctx.time = time_us_64();
        return true;
    }

    if (!req_ctx.active) {
        return false;
    }

    if (c == 0xd0) {
        req_ctx.escaping = true;
        return true;
    }

    if (req_ctx.escaping) {
        c++;
        req_ctx.escaping = false;
    }

    if (req_ctx.len != 0 && req_ctx.len == request.len) {
        if (req_ctx.check_sum == c) {
            aime_handle_frame();
            req_ctx.active = false;
        }
        return true;
    }

    request.raw[req_ctx.len] = c;
    req_ctx.len++;
    req_ctx.check_sum += c;

    return true;
}

void aime_update()
{
    if (req_ctx.active && time_us_64() - req_ctx.time > FRAME_TIMEOUT) {
        req_ctx.active = false;
    }

    if (tud_cdc_n_available(aime_interface)) {
        uint8_t buf[32];
        uint32_t count = tud_cdc_n_read(aime_interface, buf, sizeof(buf));
        for (int i = 0; i < count; i++) {
            aime_feed(buf[i]);
        }
    }
}

uint32_t aime_led_color()
{
    return led_color;
}
