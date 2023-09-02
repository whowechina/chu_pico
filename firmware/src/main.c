/*
 * Controller Main
 * WHowe <github.com/whowechina>
 */

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "bsp/board.h"
#include "pico/multicore.h"
#include "pico/stdio.h"

#include "tusb.h"
#include "usb_descriptors.h"

#include "slider.h"
#include "air.h"
#include "rgb.h"

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
    while (1) {
        rgb_update();
        sleep_ms(1);
    }
}

static void core0_loop()
{
    while(1) {
        tud_task();
        hid_report.buttons = 0xcccc;
        report_usb_hid();

        slider_update();
        air_update();

        for (int i = 0; i < 32; i++) {
            bool v = slider_touched(i);
            printf("%d", v);
        }

        for (int i = 0; i < air_num(); i++) {
            uint8_t v = air_value(i) >> 4;
            printf(" %3d", air_value(i));
            if (v == 255) {
                rgb_set_color(31 + i, 0);
            } else {
                rgb_set_color(31 + i, rgb32(v, v, v, true));
            }
        }
        printf("\n");

        sleep_ms(1);
    }
}

void init()
{
    sleep_ms(200);
    board_init();
    tusb_init();
    stdio_init_all();
    slider_init();
    air_init();
    rgb_init();
}

int main(void)
{
    init();
    multicore_launch_core1(core1_loop);
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
