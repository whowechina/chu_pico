/*
 * Controller Main
 * WHowe <github.com/whowechina>
 */

#include <stdint.h>
#include <stdbool.h>

#include "bsp/board.h"
#include "pico/multicore.h"
#include "pico/bootrom.h"
#include "pico/stdio.h"
#include "hardware/watchdog.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "hardware/i2c.h"

#include "i2c_hub.h"
#include "gp2y0e.h"
#include "vl6180.h"

#include "mpr121.h"
#include "tusb.h"
#include "usb_descriptors.h"

/* Measure the time of a function call */
#define RUN_TIME(func) \
   { uint64_t _t = time_us_64(); func; \
     printf(#func ":%lld\n", time_us_64() - _t); }

struct {
    uint16_t buttons;
    uint8_t joy[6];
} hid_report;

void report_usb_hid()
{
    if (tud_hid_ready()) {
        hid_report.joy[2] = 0;
        hid_report.joy[3] = 64;
        hid_report.joy[4] = 128;
        hid_report.joy[5] = 192;
        tud_hid_n_report(0x00, REPORT_ID_JOYSTICK, &hid_report, sizeof(hid_report));
    }
}

static bool request_core1_pause = false;

static void pause_core1(bool pause)
{
    request_core1_pause = pause;
    if (pause) {
        sleep_ms(5); /* wait for any IO ops to finish */
    }
}

static void core1_loop()
{
}


// I2C definitions: port and pin numbers
#define MPR121_PORT i2c0
#define MPR121_SDA 16
#define MPR121_SCL 17

// MPR121 I2C definitions: address and frequency.
#define MPR121_ADDR 0x5A
#define MPR121_I2C_FREQ 400000

// Touch and release thresholds.
#define MPR121_TOUCH_THRESHOLD 16
#define MPR121_RELEASE_THRESHOLD 10

#define I2C_PORT i2c0
#define I2C_I2C_FREQ 200000
#define I2C_SDA 16
#define I2C_SCL 17
#define I2C_HUB 19

static void core0_loop_gp2y()
{
    sleep_ms(100);

    i2c_init(I2C_PORT, I2C_I2C_FREQ);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    gpio_init(I2C_HUB);
    gpio_set_dir(I2C_HUB, GPIO_OUT);
    gpio_put(I2C_HUB, 1);

    i2c_select(I2C_PORT, 0xff);
    gp2y0e_write(I2C_PORT, 0xa8, 0x00);

    sleep_ms(100);

    //uint8_t od1 = 0, od2 = 0, od3 = 0;
    while(1) {
        tud_task();

        hid_report.buttons = 0xcccc;
        report_usb_hid();

        i2c_select(I2C_PORT, 0xff);
        uint8_t d1 = gp2y0e_dist(I2C_PORT);
        printf("%3d\n", d1);
    }
}

static void core0_loop_adc()
{
    adc_init();
    adc_gpio_init(28);
    adc_select_input(2);

    while(1) {
        tud_task();

        hid_report.buttons = 0xcccc;
        report_usb_hid();

        uint16_t result = adc_read();
        printf("%6o\n", result);
        sleep_ms(1);
    }
}

static void core0_loop_mpr121()
{
    // Initialise I2C.
    i2c_init(MPR121_PORT, MPR121_I2C_FREQ);
    gpio_set_function(MPR121_SDA, GPIO_FUNC_I2C);
    gpio_set_function(MPR121_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(MPR121_SDA);
    gpio_pull_up(MPR121_SCL);

    struct mpr121_sensor mpr121[3];
    for (int m = 0; m < 3; m++) {
        mpr121_init(MPR121_PORT, MPR121_ADDR + m, mpr121 + m);
        mpr121_set_thresholds(MPR121_TOUCH_THRESHOLD,
                            MPR121_RELEASE_THRESHOLD, mpr121 + m);
        // Enable only one touch sensor (electrode 0).
        mpr121_enable_electrodes(12, mpr121 + m);
    }

    int16_t baseline[34] = {0};

    uint32_t counter[34] = {0};
    for (int c = 0; c < 2000; c++) {
        tud_task();
        hid_report.buttons = 0xcccc;
        report_usb_hid();
        for (int i = 0; i < 34; i++) {
            int16_t touch_data;
            mpr121_baseline_value(i % 12, &touch_data, mpr121 + i / 12);
            counter[i] += touch_data;
        }
    }

    for (int i = 0; i < 34; i++) {
        baseline[i] = counter[i] / 2000;
    }

    while(1) {
        tud_task();

        hid_report.buttons = 0xcccc;
        report_usb_hid();

        for (int i = 0; i < 34; i++) {
            int16_t touch_data;
            mpr121_baseline_value(i % 12, &touch_data, mpr121 + i / 12);
            int16_t display_data = (baseline[i] - touch_data) / 8;
            if (display_data) {
                printf("%2d", display_data);
            } else {
                printf("  ");
            }
            printf("%c", i % 12 == 11 ? ':' : ' ');
        }
        printf("\n");
    }
}

static void core0_loop()
{
    core0_loop_gp2y();
}

void init()
{
    board_init();
    tusb_init();

    stdio_init_all();

}

int main(void)
{
    sleep_ms(1000);

    init();
    //multicore_launch_core1(core1_loop);

    core0_loop();

    return 0;
}

// Invoked when received GET_REPORT control request
// Application must fill buffer report's content and return its length.
// Return zero will cause the stack to STALL request
uint16_t tud_hid_get_report_cb(uint8_t itf, uint8_t report_id,
                               hid_report_type_t report_type, uint8_t *buffer,
                               uint16_t reqlen)
{
    return 0;
}

// Invoked when received SET_REPORT control request or
// received data on OUT endpoint ( Report ID = 0, Type = 0 )
void tud_hid_set_report_cb(uint8_t itf, uint8_t report_id,
                           hid_report_type_t report_type, uint8_t const *buffer,
                           uint16_t bufsize)
{
    if ((report_id == REPORT_ID_LIGHTS) &&
        (report_type == HID_REPORT_TYPE_OUTPUT)) {
        if (bufsize >= 0) {
            return;
        }
    }
}
