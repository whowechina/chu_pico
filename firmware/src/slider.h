/*
 * Chu Pico Silder Keys
 * WHowe <github.com/whowechina>
 */

#ifndef Silder_H
#define Silder_H

#include <stdint.h>
#include <stdbool.h>

void slider_init();
int slider_value(unsigned key);
int slider_baseline(unsigned key);
int slider_delta(unsigned key);
bool slider_touched(unsigned key);
uint16_t slider_hw_touch(unsigned m);
void slider_update();
void slider_update_baseline();

#endif
