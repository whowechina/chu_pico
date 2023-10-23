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

    int result = read_frame(resp, 6);

    const uint8_t expect_ack[] = {0, 0, 0xff, 0, 0xff, 0};
    if (memcmp(resp, expect_ack, 6) != 0) {
        return false;
    }
   
    return true;
}

int pn532_write_data(const uint8_t *data, uint8_t len)
{
    uint8_t frame[40];
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

    int result = write_frame(frame, 7 + len);

    return read_ack();
}

int pn532_read_data(uint8_t *data, uint8_t len)
{
    uint8_t resp[len + 7];

    int result = read_frame(resp, len + 7);

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

    for (int i = 0; i < len; i++) {
        data[2 + i] = param[i];
    }

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

bool pn532_config_sam()
{
    uint8_t param[] = {0x01, 0x14, 0x01};
    int result = pn532_write_command(0x14, param, 3);

    return pn532_read_response(0x14, NULL, 0) == 0;
}


bool pn532_set_rf_field(uint8_t auto_rf, uint8_t on_off)
{
    uint8_t param[] = { 1, auto_rf | on_off };
    int result = pn532_write_command(0x32, param, 2);

    return pn532_read_response(0x32, NULL, 0) >= 0;
}

static uint8_t card[32];

bool pn532_poll_mifare(uint8_t *uid, int *len)
{
    uint8_t param[] = {0x01, 0x00};
    int ret = pn532_write_command(0x4a, param, sizeof(param));
    if (ret < 0) {
        return false;
    }

    int result = pn532_read_response(0x4a, card, sizeof(card));
    if (result < 1 || card[0] != 1) {
        return false;
    }

    if (result != card[5] + 6) {
        return false;
    }

    if (*len < card[5]) {
        return false;
    }

    memcpy(uid, card + 6, card[5]);
    *len = card[5];

    return true;
}

bool pn532_poll_felica(uint8_t *uid, int *len)
{
    uint8_t param[] = { 1, 1, 0, 0xff, 0xff, 1, 0};
    int ret = pn532_write_command(0x4a, param, sizeof(param));
    if (ret < 0) {
        return false;
    }

    int result = pn532_read_response(0x4a, card, sizeof(card));
    if (card[0] != 1) {
        return false;
    }

    if ((result == 20 && card[2] == 18) ||
        (result == 22 && card[2] == 20)) {
        if (*len < card[2]) {
            return false;
        }
        *len = card[2];
        memcpy(uid, card + 4, card[2]);
        return true;
    }

    return false;
}

#if 0
bool pn532_felica_read_no_encrypt(uint16_t service_code, uint16_t block,
                                  uint8_t block_data[16])
{
    uint8_t i, j=0, k;
    uint8_t cmdLen = 1 + 8 + 1 + 2 + 1 + 2;
    uint8_t cmd[cmdLen];
    cmd[j++] = FELICA_CMD_READ_WITHOUT_ENCRYPTION;
    for (i=0; i<8; ++i) {
    cmd[j++] = _felicaIDm[i];
    }
    cmd[j++] = numService;
    for (i=0; i<numService; ++i) {
    cmd[j++] = serviceCodeList[i] & 0xFF;
    cmd[j++] = (serviceCodeList[i] >> 8) & 0xff;
    }
    cmd[j++] = numBlock;
    for (i=0; i<numBlock; ++i) {
    cmd[j++] = (blockList[i] >> 8) & 0xFF;
    cmd[j++] = blockList[i] & 0xff;
    }

    uint8_t response[12+16*numBlock];
    uint8_t responseLength;
    if (felica_SendCommand(cmd, cmdLen, response, &responseLength) != 1) {
    DMSG("Read Without Encryption command failed\n");
    return -3;
    }

    // length check
    if ( responseLength != 12+16*numBlock ) {
    DMSG("Read Without Encryption command failed (wrong response length)\n");
    return -4;
    }

    // status flag check
    if ( response[9] != 0 || response[10] != 0 ) {
    DMSG("Read Without Encryption command failed (Status Flag: ");
    DMSG_HEX(pn532_packetbuffer[9]);
    DMSG_HEX(pn532_packetbuffer[10]);
    DMSG(")\n");
    return -5;
    }

    k = 12;
    for(i=0; i<numBlock; i++ ) {
    for(j=0; j<16; j++ ) {
        blockData[i][j] = response[k++];
    }
    }

    return 1;
}
#endif
