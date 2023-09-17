/*
 * Chu Pico Air Sensor
 * WHowe <github.com/whowechina>
 * 
 * Use ToF (Sharp GP2Y0E03) to detect air keys
 */

#include "air.h"

#include <stdint.h>
#include <stdbool.h>

#include "bsp/board.h"
#include "hardware/gpio.h"

#include "board_defs.h"
#include "config.h"

#include "gp2y0e.h"
#include "i2c_hub.h"

static const uint8_t TOF_LIST[] = TOF_MUX_LIST;
static uint16_t distances[sizeof(TOF_LIST)];

void air_init()
{
    i2c_init(I2C_PORT, I2C_FREQ);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    i2c_hub_init();

    for (int i = 0; i < sizeof(TOF_LIST); i++) {
        i2c_select(I2C_PORT, 1 << TOF_LIST[i]);
        gp2y0e_write(I2C_PORT, 0xa8, 0); // Accumulation 0:1, 1:5, 2:30, 3:10
        gp2y0e_write(I2C_PORT, 0x3f, 0x30); // Filter 0x00:7, 0x10:5, 0x20:9, 0x30:1
        gp2y0e_write(I2C_PORT, 0x13, 5); // Pulse [3..7]:[40, 80, 160, 240, 320] us
    }
}

size_t air_num()
{
    return sizeof(TOF_LIST);
}

static inline uint8_t air_bits(int dist, int offset)
{
    if ((dist < offset) || (dist == 4095)) {
        return 0;
    }

    int pitch = chu_cfg->tof.pitch * 10;
    int index = (dist - offset) / pitch;
    if (index >= 6) {
        return 0;
    }

    return 1 << index;
}

uint8_t air_bitmap()
{
    int offset = chu_cfg->tof.offset * 10;
    int pitch = chu_cfg->tof.pitch * 10;
    uint8_t bitmap = 0;
    for (int i = 0; i < sizeof(TOF_LIST); i++) {
        bitmap |= air_bits(distances[i], offset) |
                  air_bits(distances[i], offset + pitch);
    }
    return bitmap;
}

unsigned air_value(uint8_t index)
{
    if (index >= sizeof(TOF_LIST)) {
        return 0;
    }
    int offset = chu_cfg->tof.offset * 10;
    int pitch = chu_cfg->tof.pitch * 10;
    uint8_t bitmap = air_bits(distances[index], offset) |
                     air_bits(distances[index], offset + pitch);

    for (int i = 0; i < 6; i++) {
        if (bitmap & (1 << i)) {
            return i + 1; // lowest detected position
        }
    }

    return 0;
}

void air_update()
{
    for (int i = 0; i < sizeof(TOF_LIST); i++) {
        i2c_select(I2C_PORT, 1 << TOF_LIST[i]);
        distances[i] = gp2y0e_dist16(I2C_PORT);
    }
}

