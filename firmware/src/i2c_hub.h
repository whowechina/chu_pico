/*
 * I2C Hub Using PCA9548A
 * WHowe <github.com/whowechina>
 */


#ifndef I2C_HUB_H
#define I2C_HUB_H

#include "hardware/i2c.h"
#include "board_defs.h"

#define I2C_HUB_ADDR 0x70
static inline void i2c_hub_init()
{
    // pull up gpio TOF_I2C_HUB
    gpio_init(TOF_I2C_HUB);
    gpio_set_dir(TOF_I2C_HUB, GPIO_OUT);
    gpio_put(TOF_I2C_HUB, 1);
}

static inline void i2c_select(i2c_inst_t *i2c_port, uint8_t chn)
{
    sleep_us(10);
    i2c_write_blocking_until(i2c_port, I2C_HUB_ADDR, &chn, 1, false, time_us_64() + 1000);
    sleep_us(10);
}

#endif
