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
#include "vl53l0x.h"
#include "i2c_hub.h"

static const uint8_t TOF_LIST[] = TOF_MUX_LIST;
static uint8_t tof_model[sizeof(TOF_LIST)];
static uint16_t distances[sizeof(TOF_LIST)];

void air_init()
{
    i2c_init(I2C_PORT, I2C_FREQ);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    i2c_hub_init();
    vl53l0x_init(I2C_PORT, 0);

    for (int i = 0; i < sizeof(TOF_LIST); i++) {
        i2c_select(I2C_PORT, 1 << TOF_LIST[i]);
        if (vl53l0x_is_present()) {
            tof_model[i] = 1;
        } else if (gp2y0e_is_present(I2C_PORT)) {
            tof_model[i] = 2;
        } else {
            tof_model[i] = 0;
        }
        if (tof_model[i] == 1) {
            vl53l0x_init_tof(true);
            vl53l0x_start_continuous();
        } else if (tof_model[i] == 2) {
            gp2y03_init(I2C_PORT);
        }
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
        if (tof_model[i] == 1) {
            distances[i] = readRangeContinuousMillimeters() * 10;
        } else if (tof_model[i] == 2) {
            distances[i] = gp2y0e_dist16_mm(I2C_PORT) * 10;
        }
    }
}

uint16_t air_raw(uint8_t index)
{
    if (index >= sizeof(TOF_LIST)) {
        return 0;
    }
    return distances[index];
}
