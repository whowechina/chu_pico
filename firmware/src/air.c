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
        gp2y0e_write(TOF_I2C, 0xa8, 0x00); // Accumulation 0:1, 1:5, 2:30, 3:10
        gp2y0e_write(TOF_I2C, 0x3f, 0x30); // Filter 0x00:7, 0x10:5, 0x20:9, 0x30:1
        gp2y0e_write(TOF_I2C, 0x13, 0x03); // Pulse [3..7]:[40, 80, 160, 240, 320] us
    }
}

size_t air_num()
{
    return sizeof(TOF_LIST);
}

uint16_t air_value(uint8_t index)
{
    if (index >= sizeof(TOF_LIST)) {
        return 0;
    }
    return distances[index];
}

void air_update()
{
    for (int i = 0; i < sizeof(TOF_LIST); i++) {
        i2c_select(TOF_I2C, 1 << TOF_LIST[i]);
        distances[i] = gp2y0e_dist16(TOF_I2C);
    }
}

