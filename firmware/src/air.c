/*
 * Chu Pico Air Sensor
 * WHowe <github.com/whowechina>
 * 
 * Use ToF (Sharp GP2Y0E03) to detect air keys
 */

#include "air.h"

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#include "hardware/gpio.h"
#include "hardware/adc.h"

#include "board_defs.h"
#include "config.h"

#include "gp2y0e.h"
#include "vl53l0x.h"
#include "i2c_hub.h"

static const uint8_t TOF_LIST[] = TOF_MUX_LIST;
static uint8_t tof_model[count_of(TOF_LIST)];
static uint16_t distances[count_of(TOF_LIST)];

static const uint8_t IR_ABC[] = IR_GROUP_ABC_GPIO;
static const uint8_t IR_SIG[] = IR_SIG_ADC_CHANNEL;
static_assert(count_of(IR_ABC) == 3, "IR should have 3 groups");
static_assert(count_of(IR_SIG) == 2, "IR should use 2 analog signals");
static uint16_t ir_raw[6];
static bool ir_blocked[6];
#define IR_DEBOUNCE_PERCENT 90

static void air_init_tof()
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
            vl53l0x_init_tof(i);
            vl53l0x_start_continuous(i);
        } else if (tof_model[i] == 2) {
            gp2y03_init(I2C_PORT);
        }
    }
}

static void air_init_ir()
{
    for (int i = 0; i < count_of(IR_ABC); i++) {
        gpio_init(IR_ABC[i]);
        gpio_set_dir(IR_ABC[i], GPIO_OUT);
        gpio_put(IR_ABC[i], 0);
    }
    for (int i = 0; i < count_of(IR_SIG); i++) {
        adc_init();
        adc_gpio_init(26 + IR_SIG[i]);
    }
}

void air_init()
{
    if (chu_cfg->ir.enabled == 0) {
        air_init_tof();
    } else {
        air_init_ir();
    }
}

size_t air_tof_num()
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

static uint8_t tof_bitmap()
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

static uint8_t ir_bitmap()
{
    uint8_t bitmap = 0;
    for (int i = 0; i < count_of(ir_blocked); i++) {
        bitmap |= ir_blocked[i] << i;
    }
    return bitmap;
}

uint8_t air_bitmap()
{
    return chu_cfg->ir.enabled ? ir_bitmap() : tof_bitmap();
}

unsigned air_tof_value(uint8_t index)
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

uint16_t air_tof_raw(uint8_t index)
{
    if (index >= count_of(TOF_LIST)) {
        return 0;
    }
    return distances[index];
}

uint16_t air_ir_raw(uint8_t index)
{
    if (index >= count_of(ir_raw)) {
        return 0;
    }
    return ir_raw[index];
}

static void air_update_tof()
{
    for (int i = 0; i < sizeof(TOF_LIST); i++) {
        i2c_select(I2C_PORT, 1 << TOF_LIST[i]);
        if (tof_model[i] == 1) {
            distances[i] = readRangeContinuousMillimeters(i) * 10;
        } else if (tof_model[i] == 2) {
            distances[i] = gp2y0e_dist16_mm(I2C_PORT) * 10;
        }
    }
}

static void ir_read()
{
    for (int i = 0; i < count_of(IR_ABC); i++) {
        gpio_put(IR_ABC[i], 1);
        sleep_us(9);

        adc_select_input(IR_SIG[0]);
        sleep_us(1);
        ir_raw[i * 2] = adc_read();

        adc_select_input(IR_SIG[1]);
        sleep_us(1);
        ir_raw[i * 2 + 1] = adc_read();

        gpio_put(IR_ABC[i], 0);
        sleep_us(1);
    }
}

static void ir_judge()
{
    for (int i = 0; i < count_of(ir_raw); i++) {
        int offset = chu_cfg->ir.base[i] - ir_raw[i];
        int threshold = chu_cfg->ir.base[i] * chu_cfg->ir.trigger[i] / 100;

        if (ir_blocked[i]) {
            threshold = threshold * IR_DEBOUNCE_PERCENT / 100;
        }

        ir_blocked[i] = offset >= threshold;
    }
}

static void ir_diagnostic()
{
    if (!chu_runtime.ir_diagnostics) {
        return;
    }

    static uint64_t last_print = 0;
    uint64_t now = time_us_64();
    if (now - last_print > 500000) {
        printf("IR: ");
        for (int i = 0; i < count_of(ir_raw); i++) {
            printf(" %4d", ir_raw[i]);
        }
        printf("\n");
        last_print = now;
    }
}

static void air_update_ir()
{
    ir_read();
    ir_judge();
    ir_diagnostic();
}

void air_update()
{
    if (chu_cfg->ir.enabled) {
        air_update_ir();
    } else {
        air_update_tof();
    }
}
