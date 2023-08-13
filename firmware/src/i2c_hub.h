/*
 * I2C Hub Using PCA9548A
 * WHowe <github.com/whowechina>
 */


#ifndef I2C_HUB_H
#define I2C_HUB_H

#include "hardware/i2c.h"

#define I2C_HUB_ADDR 0x70

static inline void i2c_select(i2c_inst_t *i2c_port, uint8_t chn)
{
    sleep_us(10);
    i2c_write_blocking(i2c_port, I2C_HUB_ADDR, &chn, 1, false);
    sleep_us(10);
}

#endif
