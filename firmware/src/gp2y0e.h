/*
 * GP2Y0E Distance measurement sensor
 * WHowe <github.com/whowechina>
 */

#ifndef GP2Y0E_H
#define GP2Y0E_H

#include <stdint.h>
#include "hardware/i2c.h"

#define GP2Y0E_DEF_ADDR 0x40

static inline void gp2y0e_write(i2c_inst_t *port, uint8_t addr, uint8_t val)
{
    uint8_t cmd[] = {addr, val};
    i2c_write_blocking_until(port, GP2Y0E_DEF_ADDR, cmd, 2, false, time_us_64() + 1000);
}

static inline void gp2y03_init(i2c_inst_t *port)
{
    gp2y0e_write(port, 0xa8, 0); // Accumulation 0:1, 1:5, 2:30, 3:10
    gp2y0e_write(port, 0x3f, 0x30); // Filter 0x00:7, 0x10:5, 0x20:9, 0x30:1
    gp2y0e_write(port, 0x13, 5); // Pulse [3..7]:[40, 80, 160, 240, 320] us
}

static inline bool gp2y0e_is_present(i2c_inst_t *port)
{
    uint8_t cmd[] = {0x5e};
    return i2c_write_blocking_until(port, GP2Y0E_DEF_ADDR, cmd, 1, true,
                                    time_us_64() + 1000) == 1;
}

static inline uint8_t gp2y0e_dist_mm(i2c_inst_t *port)
{
    uint8_t cmd[] = {0x5e};
    i2c_write_blocking_until(port, GP2Y0E_DEF_ADDR, cmd, 1, true, time_us_64() + 1000);
    uint8_t data;
    i2c_read_blocking_until(port, GP2Y0E_DEF_ADDR, &data, 1, false, time_us_64() + 1000);

    return data * 10 / 4;
}

static inline uint16_t gp2y0e_dist16_mm(i2c_inst_t *port)
{
    uint8_t cmd[] = {0x5e};
    i2c_write_blocking_until(port, GP2Y0E_DEF_ADDR, cmd, 1, true, time_us_64() + 1000);
    uint8_t data[2];
    i2c_read_blocking_until(port, GP2Y0E_DEF_ADDR, data, 2, false, time_us_64() + 1000);

    return ((data[0] << 4) | data[1]) * 10 / 64;
}

#endif