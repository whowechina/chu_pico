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
int slider_delta(unsigned key);
bool slider_touched(unsigned key);
void slider_update();

#endif
