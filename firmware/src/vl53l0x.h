/*
 * VL53L0X Distance measurement sensor
 * WHowe <github.com/whowechina>
 */

#ifndef VL53L0X_H
#define VL53L0X_H

#include <stdint.h>

int tofGetModel(int *model, int *revision);
int vl53l0x_distance();
int vl53l0x_init();

#endif