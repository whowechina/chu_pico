/*
 * VL6180X Distance measurement sensor
 * WHowe <github.com/whowechina>
 */

#ifndef VL6180_H
#define VL6180_H

#include "hardware/i2c.h"

#define VL6180_DEF_ADDR 0x29

void vl6180_write(i2c_inst_t *i2c_port, uint16_t addr, uint8_t data);
uint8_t vl6180_read(i2c_inst_t *i2c_port, uint16_t addr);
uint16_t vl6180_read16(i2c_inst_t *i2c_port, uint16_t addr);

void vl6180_init(i2c_inst_t *i2c_port);
void vl6180_continous(i2c_inst_t *i2c_port);
uint8_t vl6180_dist(i2c_inst_t *i2c_port, uint8_t old);
uint16_t vl6180_dist16(i2c_inst_t *i2c_port);

#endif