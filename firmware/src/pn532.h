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

bool pn532_config_sam();
bool pn532_poll_mifare(uint8_t *uid, int *len);
bool pn532_poll_felica(uint8_t *uid, int *len);

#endif
