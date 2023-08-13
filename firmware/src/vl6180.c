/*
 * VL6180X Distance measurement sensor
 * WHowe <github.com/whowechina>
 */

#ifndef VL6180_H
#define VL6180_H

#include "hardware/i2c.h"
#include "vl6180.h"

#define VL6180_DEF_ADDR 0x29

void vl6180_write(i2c_inst_t *i2c_port, uint16_t addr, uint8_t data)
{
    uint8_t cmd[] = {addr >> 8, addr & 0xff, data};
    i2c_write_blocking(i2c_port, VL6180_DEF_ADDR, cmd, 3, false);
}

uint8_t vl6180_read(i2c_inst_t *i2c_port, uint16_t addr)
{
    uint8_t cmd[] = {addr >> 8, addr & 0xff, 0};
    i2c_write_blocking(i2c_port, VL6180_DEF_ADDR, cmd, 3, true);
    sleep_us(1);
    uint8_t data;
    i2c_read_blocking(i2c_port, VL6180_DEF_ADDR, &data, 1, false);
    return data;
}

uint16_t vl6180_read16(i2c_inst_t *i2c_port, uint16_t addr)
{
    uint8_t cmd[] = {addr >> 8, addr & 0xff};
    i2c_write_blocking(i2c_port, VL6180_DEF_ADDR, cmd, 2, true);
    uint8_t data[2];
    i2c_read_blocking(i2c_port, VL6180_DEF_ADDR, data, 2, false);
    return data[0] << 8 | data[1];
}

void vl6180_init(i2c_inst_t *i2c_port)
{
    if (vl6180_read(i2c_port, 0x016) == 1) // reset flag
    {
        vl6180_write(i2c_port, 0x207, 0x01);
        vl6180_write(i2c_port, 0x208, 0x01);
        vl6180_write(i2c_port, 0x096, 0x00);
        vl6180_write(i2c_port, 0x097, 0xFD); // RANGE_SCALER = 253
        vl6180_write(i2c_port, 0x0E3, 0x00);
        vl6180_write(i2c_port, 0x0E4, 0x04);
        vl6180_write(i2c_port, 0x0E5, 0x02);
        vl6180_write(i2c_port, 0x0E6, 0x01);
        vl6180_write(i2c_port, 0x0E7, 0x03);
        vl6180_write(i2c_port, 0x0F5, 0x02);
        vl6180_write(i2c_port, 0x0D9, 0x05);
        vl6180_write(i2c_port, 0x0DB, 0xCE);
        vl6180_write(i2c_port, 0x0DC, 0x03);
        vl6180_write(i2c_port, 0x0DD, 0xF8);
        vl6180_write(i2c_port, 0x09F, 0x00);
        vl6180_write(i2c_port, 0x0A3, 0x3C);
        vl6180_write(i2c_port, 0x0B7, 0x00);
        vl6180_write(i2c_port, 0x0BB, 0x3C);
        vl6180_write(i2c_port, 0x0B2, 0x09);
        vl6180_write(i2c_port, 0x0CA, 0x09);
        vl6180_write(i2c_port, 0x198, 0x01);
        vl6180_write(i2c_port, 0x1B0, 0x17);
        vl6180_write(i2c_port, 0x1AD, 0x00);
        vl6180_write(i2c_port, 0x0FF, 0x05);
        vl6180_write(i2c_port, 0x100, 0x05);
        vl6180_write(i2c_port, 0x199, 0x05);
        vl6180_write(i2c_port, 0x1A6, 0x1B);
        vl6180_write(i2c_port, 0x1AC, 0x3E);
        vl6180_write(i2c_port, 0x1A7, 0x1F);
        vl6180_write(i2c_port, 0x030, 0x00);

        vl6180_write(i2c_port, 0x011, 0x10);
        vl6180_write(i2c_port, 0x10a, 0x30);
        vl6180_write(i2c_port, 0x03f, 0x46);
        vl6180_write(i2c_port, 0x031, 0xff);
        vl6180_write(i2c_port, 0x040, 0x63);
        vl6180_write(i2c_port, 0x02e, 0x01);
        vl6180_write(i2c_port, 0x01b, 0x00);
        vl6180_write(i2c_port, 0x01c, 0x01);

        vl6180_write(i2c_port, 0x03e, 0x31);
        vl6180_write(i2c_port, 0x014, 0x24);

        vl6180_write(i2c_port, 0x016, 0);
    }
}

void vl6180_continous(i2c_inst_t *i2c_port)
{
    while (vl6180_read(i2c_port, 0x04d) & 1 == 0);

    vl6180_write(i2c_port, 0x018, 3); // start/stop continous mode
}

uint8_t vl6180_dist(i2c_inst_t *i2c_port, uint8_t old)
{
    while (vl6180_read(i2c_port, 0x04d) & 1 == 0);
    return vl6180_read(i2c_port, 0x064);
}

uint16_t vl6180_dist16(i2c_inst_t *i2c_port)
{
    return vl6180_read16(i2c_port, 0x064);
}

#endif