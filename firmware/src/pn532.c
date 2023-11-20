/*
 * PN532 NFC Reader
 * WHowe <github.com/whowechina>
 *
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "hardware/i2c.h"
#include "hardware/gpio.h"

//#define DEBUG

#include "pn532.h"
#include "board_defs.h"

#define IO_TIMEOUT_US 1000
#define PN532_I2C_ADDRESS 0x24

#define PN532_PREAMBLE 0
#define PN532_STARTCODE1 0
#define PN532_STARTCODE2 0xff
#define PN532_POSTAMBLE 0

#define PN532_HOSTTOPN532 0xd4
#define PN532_PN532TOHOST 0xd5

void pn532_init()
{
    i2c_init(I2C_PORT, I2C_FREQ);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
}

static int pn532_write(const uint8_t *data, uint8_t len)
{
    return i2c_write_blocking_until(I2C_PORT, PN532_I2C_ADDRESS, data, len, false,
                             time_us_64() + IO_TIMEOUT_US * len);
}

static int pn532_read(uint8_t *data, uint8_t len)
{
    return i2c_read_blocking_until(I2C_PORT, PN532_I2C_ADDRESS, data, len, false,
                            time_us_64() + IO_TIMEOUT_US * len);
}

static bool pn532_wait_ready()
{
    uint8_t status = 0;

    for (int retry = 0; retry < 20; retry++) {
        if (pn532_read(&status, 1) == 1 && status == 0x01) {
            return true;
        }
        sleep_us(1000);
    }

    return false;
}

static int read_frame(uint8_t *frame, uint8_t len)
{
    uint8_t buf[len + 1];
    int ret = pn532_read(buf, len + 1);

#ifdef DEBUG
    printf("I2C data read: %d -", ret);
    for (int i = 0; i < len + 1; i++) {
        printf(" %02x", buf[i]);
    }
    printf("\n");
#endif

    if (ret == len + 1) {
        memcpy(frame, buf + 1, len);
        return len;
    }
    return ret;
}

static int write_frame(const uint8_t *frame, uint8_t len)
{
    #ifdef DEBUG
        printf("I2C frame write: %d -", len);
        for (int i = 0; i < len; i++) {
            printf(" %02x", frame[i]);
        }
        printf("\n");
    #endif

    return pn532_write(frame, len);
}

static bool read_ack()
{
    uint8_t resp[6];

    if (!pn532_wait_ready()) {
        return false;
    }

    read_frame(resp, 6);

    const uint8_t expect_ack[] = {0, 0, 0xff, 0, 0xff, 0};
    if (memcmp(resp, expect_ack, 6) != 0) {
        return false;
    }
   
    return true;
}

int pn532_write_data(const uint8_t *data, uint8_t len)
{
    uint8_t frame[5 + len];
    frame[0] = PN532_PREAMBLE;
    frame[1] = PN532_STARTCODE1;
    frame[2] = PN532_STARTCODE2;
    uint8_t checksum = 0xff;

    frame[3] = len;
    frame[4] = (~len + 1);

    for (int i = 0; i < len; i++) {
        frame[5 + i] = data[i];
        checksum += data[i];
    }

    frame[5 + len] = ~checksum;
    frame[6 + len] = PN532_POSTAMBLE;

    write_frame(frame, 7 + len);

    return read_ack();
}

int pn532_read_data(uint8_t *data, uint8_t len)
{
    uint8_t resp[len + 7];

    read_frame(resp, len + 7);

    if (resp[0] != PN532_PREAMBLE ||
        resp[1] != PN532_STARTCODE1 ||
        resp[2] != PN532_STARTCODE2) {
        return -1;
    }

    uint8_t length = resp[3];
    uint8_t length_check = length + resp[4];

    if (length > len ||
        length_check != 0 ||
        resp[length + 6] != PN532_POSTAMBLE) {
        return -1;
    }

    uint8_t checksum = 0;
    for (int i = 0; i <= length; i++) {
        data[i] = resp[5 + i];
        checksum += resp[5 + i];
    }

    if (checksum != 0) {
        return -1;
    }
    
    return length;
}

int pn532_write_command(uint8_t cmd, const uint8_t *param, uint8_t len)
{
    uint8_t data[len + 2];
    data[0] = PN532_HOSTTOPN532;
    data[1] = cmd;

    memcpy(data + 2, param, len);

    return pn532_write_data(data, len + 2);
}

static void write_nack()
{
    const uint8_t nack[] = {0, 0, 0xff, 0xff, 0, 0};
    pn532_write(nack, 6);
}

int pn532_peak_response_len()
{
    uint8_t buf[6];
    if (!pn532_wait_ready()) {
        return -1;
    }
    pn532_read(buf, 6);
    if (buf[0] != 0x01 ||
        buf[1] != PN532_PREAMBLE ||
        buf[2] != PN532_STARTCODE1 ||
        buf[3] != PN532_STARTCODE2) {
        return -1;
    }

    write_nack();
    return buf[4];
}

int pn532_read_response(uint8_t cmd, uint8_t *resp, uint8_t len)
{
    int real_len = pn532_peak_response_len();
    if (real_len < 0) {
        return -1;
    }

    if (!pn532_wait_ready()) {
        return -1;
    }

    if (real_len < 2) {
        return -1;
    }

    uint8_t data[real_len];
    int ret = pn532_read_data(data, real_len);
    if (ret != real_len ||
        data[0] != PN532_PN532TOHOST ||
        data[1] != cmd + 1) {
        return -1;
    }

    int data_len = real_len - 2;
    if (data_len > len) {
        return -1;
    }

    if (data_len > 0) {
        memcpy(resp, data + 2, data_len);
    }

    return data_len;
}

uint32_t pn532_firmware_ver()
{
    int ret = pn532_write_command(0x02, NULL, 0);
    if (ret < 0) {
        return 0;
    }

    uint8_t ver[4];
    int result = pn532_read_response(0x4a, ver, sizeof(ver));
    if (result < 4) {
        return 0;
    }

    uint32_t version = 0;
    for (int i = 0; i < 4; i++) {
        version <<= 8;
        version |= ver[i];
    }
    return version;
}

bool pn532_config_rf()
{
    uint8_t param[] = {0x05, 0xff, 0x01, 0x50};
    pn532_write_command(0x32, param, sizeof(param));

    return pn532_read_response(0x32, param, sizeof(param)) == 0;
}

bool pn532_config_sam()
{
    uint8_t param[] = {0x01, 0x14, 0x01};
    pn532_write_command(0x14, param, sizeof(param));

    return pn532_read_response(0x14, NULL, 0) == 0;
}


bool pn532_set_rf_field(uint8_t auto_rf, uint8_t on_off)
{
    uint8_t param[] = { 1, auto_rf | on_off };
    pn532_write_command(0x32, param, 2);

    return pn532_read_response(0x32, NULL, 0) >= 0;
}

static uint8_t readbuf[255];

bool pn532_poll_mifare(uint8_t *uid, int *len)
{
    uint8_t param[] = {0x01, 0x00};
    int ret = pn532_write_command(0x4a, param, sizeof(param));
    if (ret < 0) {
        return false;
    }

    int result = pn532_read_response(0x4a, readbuf, sizeof(readbuf));
    if (result < 1 || readbuf[0] != 1) {
        return false;
    }

    if (result != readbuf[5] + 6) {
        return false;
    }

    if (*len < readbuf[5]) {
        return false;
    }

    memcpy(uid, readbuf + 6, readbuf[5]);
    *len = readbuf[5];

    return true;
}

bool pn532_poll_14443b(uint8_t *uid, int *len)
{
    uint8_t param[] = {0x01, 0x03, 0x00};
    int ret = pn532_write_command(0x4a, param, sizeof(param));
    if (ret < 0) {
        return false;
    }

    int result = pn532_read_response(0x4a, readbuf, sizeof(readbuf));
    if (result < 1 || readbuf[0] != 1) {
        printf("result: %d\n", result);
        return false;
    }

    if (result != readbuf[5] + 6) {
        printf("result: %d %d\n", result, readbuf[5]);
        return false;
    }

    if (*len < readbuf[5]) {
        printf("result: %d %d\n", result, readbuf[5]);
        return false;
    }

    memcpy(uid, readbuf + 6, readbuf[5]);
    *len = readbuf[5];

    return true;
}

static struct __attribute__((packed)) {
    uint8_t idm[8];
    uint8_t pmm[8];
    uint8_t syscode[2];
    uint8_t inlist_tag;
} felica_poll_cache;

bool pn532_poll_felica(uint8_t uid[8], uint8_t pmm[8], uint8_t syscode[2], bool from_cache)
{
    if (from_cache) {
        memcpy(uid, felica_poll_cache.idm, 8);
        memcpy(pmm, felica_poll_cache.pmm, 8);
        memcpy(syscode, felica_poll_cache.syscode, 2);
        return true;
    }

    uint8_t param[] = { 1, 1, 0, 0xff, 0xff, 1, 0};
    int ret = pn532_write_command(0x4a, param, sizeof(param));
    if (ret < 0) {
        return false;
    }

    int result = pn532_read_response(0x4a, readbuf, sizeof(readbuf));
    if (result != 22 || readbuf[0] != 1 || readbuf[2] != 20) {
        return false;
    }

    memcpy(&felica_poll_cache, readbuf + 4, 18);
    felica_poll_cache.inlist_tag = readbuf[1];

    memcpy(uid, readbuf + 4, 8);
    memcpy(pmm, readbuf + 12, 8);
    memcpy(syscode, readbuf + 20, 2);

    return true;
}

bool pn532_mifare_auth(const uint8_t uid[4], uint8_t block_id, uint8_t key_id, const uint8_t *key)
{
    uint8_t param[] = { 1, key_id ? 1 : 0, block_id,
                       key[0], key[1], key[2], key[3], key[4], key[5],
                       uid[0], uid[1], uid[2], uid[3] };
    int ret = pn532_write_command(0x40, param, sizeof(param));
    if (ret < 0) {
        printf("Failed mifare auth command\n");
        return false;
    }
    int result = pn532_read_response(0x40, readbuf, sizeof(readbuf));
    if (readbuf[0] != 0) {
        printf("PN532 Mifare AUTH failed %d %02x\n", result, readbuf[0]);
        return false;
    }

    return true;
}

bool pn532_mifare_read(uint8_t block_id, uint8_t block_data[16])
{
    uint8_t param[] = { 1, 0x30, block_id };

    int ret = pn532_write_command(0x40, param, sizeof(param));
    if (ret < 0) {
        printf("Failed mifare read command\n");
        return false;
    }

    int result = pn532_read_response(0x40, readbuf, sizeof(readbuf));

    if (readbuf[0] != 0 || result != 17) {
        printf("PN532 Mifare READ failed %d %02x\n", result, readbuf[0]);
        return false;
    }

    memmove(block_data, readbuf + 1, 16);

    return true;
}

int pn532_felica_command(uint8_t cmd, const uint8_t *param, uint8_t param_len, uint8_t *outbuf)
{
    int cmd_len = param_len + 11;
    uint8_t cmd_buf[cmd_len + 1];

    cmd_buf[0] = felica_poll_cache.inlist_tag;
    cmd_buf[1] = cmd_len;
    cmd_buf[2] = cmd;
    memcpy(cmd_buf + 3, felica_poll_cache.idm, 8);
    memcpy(cmd_buf + 11, param, param_len);

    int ret = pn532_write_command(0x40, cmd_buf, sizeof(cmd_buf));
    if (ret < 0) {
        printf("Failed send felica command\n");
        return -1;
    }

    int result = pn532_read_response(0x40, readbuf, sizeof(readbuf));

    int outlen = readbuf[1] - 1;
    if ((readbuf[0] & 0x3f) != 0 || result - 2 != outlen) {
        return -1;
    }

    memmove(outbuf, readbuf + 2, outlen);

    return outlen;
}


bool pn532_felica_read_wo_encrypt(uint16_t svc_code, uint16_t block_id, uint8_t block_data[16])
{
    uint8_t param[] = { 1, svc_code & 0xff, svc_code >> 8,
                        1, block_id >> 8, block_id & 0xff };

    int result = pn532_felica_command(0x06, param, sizeof(param), readbuf);

    if (result != 12 + 16 || readbuf[9] != 0 || readbuf[10] != 0) {
        printf("PN532 Felica READ read failed %d %02x %02x\n",
               result, readbuf[9], readbuf[10]);
        for (int i = 0; i < result; i++) {
            printf(" %02x", readbuf[i]);
        }
        printf("\n");
        return false;
    }

    const uint8_t *result_data = readbuf + 12; 
    memcpy(block_data, result_data, 16);

    return true;
}

bool pn532_felica_write_wo_encrypt(uint16_t svc_code, uint16_t block_id, const uint8_t block_data[16])
{
    uint8_t param[22] = { 1, svc_code & 0xff, svc_code >> 8,
                        1, block_id >> 8, block_id & 0xff };
    memcpy(param + 6, block_data, 16);
    int result = pn532_felica_command(0x08, param, sizeof(param), readbuf);

    if (result < 0) {
        printf("PN532 Felica WRITE failed %d\n", result);
        return false;
    }

    for (int i = 0; i < result; i++) {
        printf(" %02x", readbuf[i]);
    }
    printf("\n");
    return false;
}
