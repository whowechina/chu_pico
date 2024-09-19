/*
 * Chu Pico Air Sensor
 * WHowe <github.com/whowechina>
 */

#ifndef AIR_H
#define AIR_H

#include <stdlib.h>
#include <stdint.h>

void air_init();
size_t air_tof_num();
unsigned air_tof_value(uint8_t index);
uint16_t air_tof_raw(uint8_t index);
uint16_t air_ir_raw(uint8_t index);
uint8_t air_bitmap();
void air_update();

#endif
