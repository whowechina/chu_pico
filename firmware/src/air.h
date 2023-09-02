/*
 * Controller Buttons
 * WHowe <github.com/whowechina>
 */

#ifndef AIR_H
#define AIR_H

#include <stdlib.h>
#include <stdint.h>

void air_init();
size_t air_num();
uint16_t air_value(uint8_t index);
void air_update();

#endif
