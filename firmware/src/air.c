/*
 * Chu Pico air sensor
 * WHowe <github.com/whowechina>
 * 
 * Use ToF (Toshiba GP2Y0E03) to detect air keys
 */

#include "air.h"

#include <stdint.h>
#include <stdbool.h>

#include "bsp/board.h"
#include "hardware/gpio.h"

#include "board_defs.h"

#include "gp2y0e.h"
#include "i2c_hub.h"

static const uint8_t TOF_LIST[] = TOF_MUX_LIST;
static uint16_t distances[sizeof(TOF_LIST)];

const int offset_a = 800;
const int offset_b = 1000;
const int pitch = 200;

void air_init()
{
    i2c_init(TOF_I2C, 400 * 1000);
    gpio_set_function(TOF_SDA, GPIO_FUNC_I2C);
    gpio_set_function(TOF_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(TOF_SDA);
    gpio_pull_up(TOF_SCL);

    i2c_hub_init();
    sleep_us(10);

    for (int i = 0; i < sizeof(TOF_LIST); i++) {
        sleep_us(10);
        i2c_select(TOF_I2C, 1 << TOF_LIST[i]);
        gp2y0e_write(TOF_I2C, 0xa8, 0); // Accumulation 0:1, 1:5, 2:30, 3:10
        gp2y0e_write(TOF_I2C, 0x3f, 0x30); // Filter 0x00:7, 0x10:5, 0x20:9, 0x30:1
        gp2y0e_write(TOF_I2C, 0x13, 5); // Pulse [3..7]:[40, 80, 160, 240, 320] us
    }
}

size_t air_num()
{
    return sizeof(TOF_LIST);
}

static inline uint8_t air_bits(int dist, int offset)
{
    if (dist < offset) {
        return 0;
    }
    int index = (dist - offset) / pitch;
    if (index >= 6) {
        return 0;
    }

    return 1 << index;
}

uint8_t air_bitmap()
{
    uint8_t bitmap = 0;
    for (int i = 0; i < sizeof(TOF_LIST); i++) {
        bitmap |= air_bits(distances[i], offset_a);
        bitmap |= air_bits(distances[i], offset_b);
    }
    return bitmap;
}

unsigned air_value(uint8_t index)
{
    if (index >= sizeof(TOF_LIST)) {
        return 0;
    }

    uint8_t bitmap = air_bits(distances[index], offset_a) |
                     air_bits(distances[index], offset_b);

    for (int i = 0; i < 6; i++) {
        if (bitmap & (1 << i)) {
            return i + 1;
        }
    }

    return 0;
}

void air_update()
{
    for (int i = 0; i < sizeof(TOF_LIST); i++) {
        i2c_select(TOF_I2C, 1 << TOF_LIST[i]);
        distances[i] = gp2y0e_dist16(TOF_I2C);
    }
}

