/*
 * Chu Pico Slider Keys
 * WHowe <github.com/whowechina>
 */

#ifndef Slider_H
#define Slider_H

#include <stdint.h>
#include <stdbool.h>

void slider_init();
void slider_sensor_init();
void slider_update();
bool slider_touched(unsigned key);
const uint16_t *slider_raw();
void slider_update_config();
unsigned slider_count(unsigned key);
void slider_reset_stat();
const char *slider_sensor_status();


#endif
