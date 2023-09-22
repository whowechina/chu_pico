/*
 * Chu Pico Silder Keys
 * WHowe <github.com/whowechina>
 */

#ifndef Silder_H
#define Silder_H

#include <stdint.h>
#include <stdbool.h>

void slider_init();
void slider_update();
bool slider_touched(unsigned key);
void slider_update_config();

#endif
