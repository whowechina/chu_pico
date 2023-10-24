/*
 * PN532 NFC Reader
 * WHowe <github.com/whowechina>
 * 
 */

#ifndef PN532_H
#define PN532_H

void pn532_init();
int pn532_write_command(uint8_t cmd, const uint8_t *param, uint8_t len);
int pn532_read_response(uint8_t cmd, uint8_t *resp, uint8_t len);

uint32_t pn532_firmware_ver();

bool pn532_config_sam();
bool pn532_set_rf_field(uint8_t auto_rf, uint8_t on_off);

bool pn532_poll_mifare(uint8_t *uid, int *len);
bool pn532_poll_felica(uint8_t uid[8], uint8_t pmm[8], uint8_t syscode[2]);
bool pn532_felica_read_no_encrypt(uint8_t svc_num, const uint16_t *svc_codes,
                                  uint8_t block_num, const uint16_t *block_list,
                                  uint8_t block_data[][16]);
#endif
