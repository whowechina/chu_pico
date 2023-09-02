/*
 * Chu Pico Silder Keys
 * WHowe <github.com/whowechina>
 * 
 * MPR121 CapSense based Keys
 */

#include "slider.h"

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

#include "bsp/board.h"
#include "hardware/gpio.h"

#include "board_defs.h"

#include "mpr121.h"

#define TOUCH_THRESHOLD 12
#define RELEASE_THRESHOLD 10

#define MPR121_ADDR 0x5C

static uint16_t baseline[32];
static uint16_t readout[32];
static uint16_t touched[3];

static struct mpr121_sensor mpr121[3];

void slider_init()
{
    i2c_init(MPR121_I2C, 400 * 1000);
    gpio_set_function(MPR121_SDA, GPIO_FUNC_I2C);
    gpio_set_function(MPR121_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(MPR121_SDA);
    gpio_pull_up(MPR121_SCL);

    for (int m = 0; m < 3; m++) {
        mpr121_init(MPR121_I2C, MPR121_ADDR + m, mpr121 + m);
        mpr121_set_thresholds(TOUCH_THRESHOLD, RELEASE_THRESHOLD, mpr121 + m);
        mpr121_enable_electrodes(12, mpr121 + m);
    }
}

void slider_update()
{
    for (int m = 0; m < 3; m++) {
        for (int i = 0; i < 12; i++) {
            int id = m * 12 + i;
            if (id >= 32) {
                break;
            }
            mpr121_filtered_data(i, readout + id, mpr121 + m);
        }
        mpr121_touched(touched + m, mpr121 + m);
    }
}

int slider_value(unsigned key)
{
    if (key >= 32) {
        return 0;
    }
    return readout[key];
}

int slider_delta(unsigned key)
{
    if (key >= 32) {
        return 0;
    }
    return readout[key] - baseline[key];
}

bool slider_touched(unsigned key)
{
    return touched[key / 12] & (1 << (key % 12));
}
