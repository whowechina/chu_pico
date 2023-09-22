/*
 * MP121 Captive Touch Sensor
 * WHowe <github.com/whowechina>
 * 
 */

#ifndef MP121_H
#define MP121_H

void mpr121_init(uint8_t addr);
uint16_t mpr121_touched(uint8_t addr);
void mpr121_debounce(uint8_t addr, uint8_t touch, uint8_t release);
void mpr121_sense(uint8_t addr, int8_t sense, int8_t *sense_keys);

#endif