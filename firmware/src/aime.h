/*
 * AIME Reader
 * WHowe <github.com/whowechina>
 */

#ifndef AIME_H
#define AIME_H

void aime_init(int interface);
void aime_update();
uint32_t aime_led_color();

// mode 0 or 1
void aime_set_baudrate(int mode);
void aime_set_virtual_aic(bool enable);

#endif
