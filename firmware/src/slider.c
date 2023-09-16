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

#include "config.h"

#define TOUCH_THRESHOLD 17
#define RELEASE_THRESHOLD 8

#define MPR121_ADDR 0x5A

static uint16_t baseline[36];
static int16_t error[36];
static uint16_t readout[36];
static bool touched[36];
static uint16_t debounce[36];
static uint16_t touch[3];

static struct mpr121_sensor mpr121[3];

#define ABS(x) ((x) < 0 ? -(x) : (x))

static void mpr121_read_many(uint8_t addr, uint8_t reg, uint8_t *buf, size_t n)
{
    i2c_write_blocking_until(I2C_PORT, addr, &reg, 1, true, time_us_64() + 2000);
    i2c_read_blocking_until(I2C_PORT, addr, buf, n, false, time_us_64() + 2000);
}

static void mpr121_read_many16(uint8_t addr, uint8_t reg, uint16_t *buf, size_t n)
{
    uint8_t vals[n * 2];
    mpr121_read_many(addr, reg, vals, n * 2);
    for (int i = 0; i < n; i++) {
        buf[i] = (vals[i * 2 + 1] << 8) | vals[i * 2];
    }
}

static void init_baseline()
{
    sleep_ms(100);
    for (int m = 0; m < 3; m++) {
        uint8_t vals[12];
        mpr121_read_many(MPR121_ADDR + m, MPR121_BASELINE_VALUE_REG, vals, 12);
        for (int i = 0; i < 12; i++) {
            baseline[m * 12 + i] = vals[i] * 4;
        }
    }
}

void slider_init()
{
    i2c_init(I2C_PORT, I2C_FREQ);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
    
    for (int m = 0; m < 3; m++) {
        mpr121_init(I2C_PORT, MPR121_ADDR + m, mpr121 + m);
    }
    
    init_baseline();
}

void slider_update()
{
    uint8_t reg = MPR121_ELECTRODE_FILTERED_DATA_REG;
    mpr121_read_many16(MPR121_ADDR, reg, readout, 12);
    mpr121_read_many16(MPR121_ADDR + 1, reg, readout + 12, 12);
    mpr121_read_many16(MPR121_ADDR + 2, reg, readout + 24, 12);
//    mpr121_touched(touch, mpr121);
//    mpr121_touched(touch + 1, mpr121 + 1);
//    mpr121_touched(touch + 2, mpr121 + 2);
}

void slider_update_baseline()
{
    static int iteration = 0;

    for (int i = 0; i < 32; i++) {
        int16_t delta = readout[i] - baseline[i];
        if (ABS(delta) > RELEASE_THRESHOLD) {
            continue;
        }
        error[i] += delta;
    }

    iteration++;
    if (iteration > 100) {
        iteration = 0;
        for (int i = 0; i < 32; i++) {
            if (error[i] > 100) {
                baseline[i] ++;
            } else if (error[i] < -100) {
                baseline[i] --;
            } else {
            }
            error[i] = 0;
        }
    }
}

int slider_value(unsigned key)
{
    if (key >= 32) {
        return 0;
    }
    return readout[key];
}

int slider_baseline(unsigned key)
{
    if (key >= 32) {
        return 0;
    }
    return baseline[key];
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
    if (key >= 32) {
        return 0;
    }
    int delta =  baseline[key] - readout[key];

    int bias = chu_cfg->sense.global + chu_cfg->sense.keys[key];
    int touch_thre = TOUCH_THRESHOLD + bias;
    int release_thre = RELEASE_THRESHOLD + bias / 2;

    if (touched[key]) {
        if (delta > touch_thre) {
            debounce[key] = 0;
        }
        if (debounce[key] > chu_cfg->sense.debounce) {
            if (delta < release_thre) {
                touched[key] = false;
            }
        } else {
            debounce[key]++;
        }
    } else if (!touched[key]) {
        if (delta > touch_thre) {
            touched[key] = true;
        }
    }

    return touched[key];
}

uint16_t slider_hw_touch(unsigned m)
{
    return touch[m];
}
