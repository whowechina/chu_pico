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

void aime_set_baudrate(int mode)
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

static uint8_t mifare_keys[2][6]; // 'KeyA' and 'KeyB'

static union __attribute__((packed)) {
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

static union __attribute__((packed)) {
    struct {
        uint8_t len;
        uint8_t addr;
        uint8_t seq;
        uint8_t cmd;
        uint8_t payload_len;
        union {
            struct {
                uint8_t uid[4];
                uint8_t block_id;
            } mifare;
            struct {
                uint8_t idm[8];
                uint8_t len;
                uint8_t code;
                uint8_t data[0];
            } felica;
            uint8_t payload[250];
        };
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

static void send_simple_response(uint8_t status)
{
    build_response(0);
    response.status = status;
    send_response();
}

static void cmd_to_normal_mode()
{
    send_simple_response(pn532_firmware_ver() ? STATUS_NFCRW_FIRMWARE_UP_TO_DATE
                                              : STATUS_NFCRW_INIT_ERROR);
}

static void cmd_fake_version(const char *version[])
{
    int len = strlen(version[baudrate_mode]);
    build_response(len);
    memcpy(response.payload, version[baudrate_mode], len);
    send_response();
}

static void cmd_key_set(uint8_t key[6])
{
    memcpy(key, request.payload, 6);
    send_simple_response(STATUS_OK);
}

static void cmd_set_polling(bool enabled)
{
    pn532_set_rf_field(0, enabled ? 1 : 0);
    send_simple_response(STATUS_OK);
}

static void cmd_detect_card()
{
    typedef struct __attribute__((packed)) {
        uint8_t count;
        uint8_t type;
        uint8_t id_len;
        union {
            struct {
                uint8_t idm[8];
                uint8_t pmm[8];
                uint8_t syscode[2];
            };
            uint8_t uid[6];
        };
    } card_info_t;
    
    card_info_t *card = (card_info_t *) response.payload;

    int len = sizeof(card->idm);
    if (pn532_poll_mifare(card->uid, &len)) {
        build_response(len > 4 ? 10 : 7);
        card->count = 1;
        card->type = 0x10;
        card->id_len = len;
        printf("Detected Mifare %d\n", card->id_len);
    } else if (pn532_poll_felica(card->idm, card->pmm, card->syscode)) {
        build_response(19);
        card->count = 1;
        card->type = 0x20;
        card->id_len = 16;
        printf("Detected Felica -");
        for (int i = 0; i < 8; i++) {
            printf(" %02x", card->idm[i]);
        }
        printf(",");
        for (int i = 0; i < 8; i++) {
            printf(" %02x", card->pmm[i]);
        }
        printf(",");
        printf(" %02x%02x\n", card->syscode[0], card->syscode[1]);
    } else {
        build_response(1);
        card->count = 0;
        response.status = STATUS_OK;
    }
    send_response();
}

static void cmd_card_select()
{
    printf("CARD SELECT %d\n", request.payload_len);
    send_simple_response(STATUS_OK);
}

static void cmd_mifare_auth(int type)
{
    printf("MIFARE AUTH\n");
    pn532_mifare_auth(request.mifare.uid, request.mifare.block_id, type, mifare_keys[type]);
    send_simple_response(STATUS_OK);
}

static void cmd_mifare_read()
{
    printf("MIFARE READ %02x %02x %02x %02x %02x\n", request.mifare.block_id,
            request.mifare.uid[0], request.mifare.uid[1], request.mifare.uid[2],
           request.mifare.uid[3]);
    build_response(16);
    memset(response.payload, 0, 16);
    pn532_mifare_read(request.mifare.block_id, response.payload);
    send_response();
}

static void cmd_mifare_halt()
{
    printf("MIFARE HALT\n");
    send_simple_response(STATUS_OK);
}

typedef struct __attribute__((packed)) {
    uint8_t len;
    uint8_t code;
    uint8_t idm[8];
    uint8_t data[0];
} felica_resp_t;

typedef struct __attribute__((packed)) {
    uint8_t idm[8];
    uint8_t pmm[8];
    uint8_t syscode[2];
} card_id_t;

static int cmd_felica_poll(const card_id_t *card)
{
    typedef struct __attribute__((packed)) {
        uint8_t pmm[8];
        uint8_t syscode[2];
    } felica_poll_resp_t;

    printf("FELICA POLL\n");

    felica_resp_t *felica_resp = (felica_resp_t *) response.payload;
    felica_poll_resp_t *poll_resp = (felica_poll_resp_t *) felica_resp->data;

    memcpy(poll_resp->pmm, card->pmm, 8);
    memcpy(poll_resp->syscode, card->syscode, 2);

    return sizeof(*poll_resp);
}

static int cmd_felica_get_syscode(const card_id_t *card)
{
    printf("FELICA GET_SYSTEM_CODE\n");
    felica_resp_t *felica_resp = (felica_resp_t *) response.payload;
    felica_resp->data[0] = 0x01;
    felica_resp->data[1] = card->syscode[0];
    felica_resp->data[2] = card->syscode[1];
    return 3;
}

static int cmd_felica_read()
{
    printf("FELICA READ\n");
    uint8_t *req_data = request.felica.data;

    if (req_data[8] != 1) {
        printf("Felica Encap READ Error: service_num != 1\n");
        return -1;
    }

    uint16_t svc_code = req_data[9] | (req_data[10] << 8);

    typedef struct __attribute__((packed)) {
        uint8_t rw_status[2];
        uint8_t block_num;
        uint8_t block_data[0][16];
    } encap_read_resp_t;

    felica_resp_t *felica_resp = (felica_resp_t *) response.payload;
    encap_read_resp_t *read_resp = (encap_read_resp_t *) felica_resp->data;

    uint8_t *block = req_data + 11;
    uint8_t block_num = block[0];

    memset(read_resp->block_data, 0, block_num * 16);

    for (int i = 0; i < block_num; i++) {
        uint16_t block_id = (block[i * 2 + 1] << 8) | block[i * 2 + 2];
        if (block_id == 0x8082) {
            memcpy(read_resp->block_data[i], felica_resp->idm, 8);
        }
        pn532_felica_read_wo_encrypt(svc_code, block_id, read_resp->block_data[i]);
    }

    read_resp->rw_status[0] = 0x00;
    read_resp->rw_status[1] = 0x00;
    read_resp->block_num = block_num;

    return sizeof(*read_resp) + block_num * 16;
}

static int cmd_felica_write()
{
    printf("FELICA WRITE\n");
    uint8_t *req_data = request.felica.data;

    if (req_data[8] != 1) {
        printf("Felica Encap Write Error: service_num != 1\n");
        return -1;
    }

    uint16_t svc_code = req_data[9] | (req_data[10] << 8);
    
    uint8_t *block = req_data + 11;
    uint8_t block_num = block[0];

    felica_resp_t *felica_resp = (felica_resp_t *) response.payload;
    uint8_t *rw_status = felica_resp->data;

    printf("svc code: %04x\n", svc_code);
    printf("blocks:");
    for (int i = 0; i < block_num; i++) {
        uint16_t block_id = (block[i * 2 + 1] << 8) | block[i * 2 + 2];
        printf(" %04x", block_id);
    }
    printf("\n");

    uint8_t *block_data = block + 1 + block_num * 2;

    rw_status[0] = 0x00;
    rw_status[1] = 0x00;

    for (int i = 0; i < block_num; i++) {
        printf("writing %02x %02x\n", block_data[i * 16], block_data[i * 16 + 15]);
        uint16_t block_id = (block[i * 2 + 1] << 8) | block[i * 2 + 2];
        int result = pn532_felica_write_wo_encrypt(svc_code, block_id, block_data + i * 16);
        if (result < 0) {
            rw_status[0] = 0x01;
            rw_status[1] = 0x01;
        }
    }

    return 2;
}

#if 0

    case CMD_FELICA_THROUGH_NDA_A4:
        printf("\tNDA_A4\n");
        build_response(11);
        resp->payload[0] = 0x00;
        break;
    default:
        printf("\tUnknown through: %02x\n", code);
        build_response(0);
        response.status = STATUS_OK;
#endif

static void cmd_felica()
{
    card_id_t card;

    if (!pn532_poll_felica(card.idm, card.pmm, card.syscode)) {
        send_simple_response(STATUS_FELICA_ERROR);        
    }

    uint8_t felica_code = request.felica.code;

    printf("Felica (%02x):", felica_code);
    for (int i = 0; i < request.payload_len; i++) {
        printf(" %02x", request.payload[i]);
    }
    printf("\n");

    int datalen = -1;
    switch (felica_code) {
        case CMD_FELICA_THROUGH_GET_SYSTEM_CODE:
            datalen = cmd_felica_get_syscode(&card);
            break;
        case CMD_FELICA_THROUGH_POLL:
            datalen = cmd_felica_poll(&card);
            break;
        case CMD_FELICA_THROUGH_READ:
            datalen = cmd_felica_read();
            break;
        case CMD_FELICA_THROUGH_WRITE:
            datalen = cmd_felica_write();
            break;
        default:
            printf("Unknown code %d\n", felica_code);
            break;
    }

    if (datalen < 0) {
        send_simple_response(STATUS_FELICA_ERROR);
        return;
    }

    felica_resp_t *felica_resp = (felica_resp_t *) response.payload;
    build_response(sizeof(*felica_resp) + datalen);
    felica_resp->len = response.payload_len;
    felica_resp->code = felica_code + 1;
    memcpy(felica_resp->idm, card.idm, 8);

    send_response();

    printf("Felica Response:");
    for (int i = 0; i < felica_resp->len - 10; i++) {
        printf(" %02x", felica_resp->data[i]);
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
            cmd_key_set(mifare_keys[0]);
            break;
        case CMD_MIFARE_KEY_SET_B:
            printf("CMD_MIFARE_KEY_SET_B\n");
            cmd_key_set(mifare_keys[1]);
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
            cmd_felica();
            break;

        case CMD_CARD_SELECT:
            cmd_card_select();
            break;
        
        case CMD_MIFARE_AUTHORIZE_A:
            cmd_mifare_auth(0);
            break;

        case CMD_MIFARE_AUTHORIZE_B:
            cmd_mifare_auth(1);
            break;
        
        case CMD_MIFARE_READ:
            cmd_mifare_read();
            break;

        case CMD_CARD_HALT:
            cmd_mifare_halt();
            break;

        case CMD_EXT_BOARD_INFO:
            cmd_fake_version(led_info);
            break;
        case CMD_EXT_BOARD_LED_RGB:
            cmd_led_rgb();
            break;

        case CMD_SEND_HEX_DATA:
        case CMD_EXT_TO_NORMAL_MODE:
            send_simple_response(STATUS_OK);
            break;

        default:
            printf("Unknown command: %02x [", request.cmd);
            for (int i = 0; i < request.len; i++) {
                printf(" %02x", request.raw[i]);
            }
            printf("]\n");
            send_simple_response(STATUS_OK);
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
