/*
 * Controller Main
 * WHowe <github.com/whowechina>
 */

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "bsp/board.h"
#include "pico/multicore.h"
#include "pico/bootrom.h"
#include "pico/stdio.h"

#include "hardware/gpio.h"
#include "hardware/sync.h"
#include "hardware/structs/ioqspi.h"
#include "hardware/structs/sio.h"

#include "tusb.h"
#include "usb_descriptors.h"

#include "slider.h"
#include "air.h"
#include "rgb.h"

/* Measure the time of a function call */
#define RUN_TIME(func) \
   { uint64_t _t = time_us_64(); func; \
     printf(#func ":%lld\n", time_us_64() - _t); }

struct __attribute__((packed)) {
    uint16_t buttons; // 16 buttons; see JoystickButtons_t for bit mapping
    uint8_t  HAT;    // HAT switch; one nibble w/ unused nibble
    uint32_t axis;  // slider touch data
    uint8_t  VendorSpec;
} hid_joy;

void report_usb_hid()
{
    if (tud_hid_ready()) {
        hid_joy.buttons = 0;
        hid_joy.HAT = 0;
        hid_joy.VendorSpec = 0;
        tud_hid_n_report(0x00, REPORT_ID_JOYSTICK, &hid_joy, sizeof(hid_joy));
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

static void fps_count()
{
    static uint64_t last = 0;
    static int fps_counter = 0;

    fps_counter++;

    uint64_t now = time_us_64();
    if (now - last < 1000000) {
        return;
    }
    last = now;
    printf("FPS: %d\n", fps_counter);
    fps_counter = 0;
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
    for (int i = 0; i < 15; i++) {
        int x = 15 - i;
        uint8_t r = (x & 0x01) ? 10 : 0;
        uint8_t g = (x & 0x02) ? 10 : 0;
        uint8_t b = (x & 0x04) ? 10 : 0;
        rgb_gap_color(i, rgb32(r, g, b, false));
    }

    while(1) {
        tud_task();
        report_usb_hid();
        slider_update();
        air_update();

#if 0
        static uint16_t old_touch = 0;
        uint16_t touch = slider_hw_touch(0);
        if (touch != old_touch) {
            printf("Touch: %04x\n", touch);
            old_touch = touch;
        }
#endif
        hid_joy.axis = 0;
        for (int i = 0; i < 16; i++) {
            bool k1 = slider_touched(i * 2);
            bool k2 = slider_touched(i * 2 + 1);

            if (k1) {
                hid_joy.axis |= 1 << (30 - i * 2);
            }
            if (k2) {
                hid_joy.axis |= 1 << (31 - i * 2);
            }

            uint8_t r = k1 ? 255 : 0;
            uint8_t g = k2 ? 255 : 0;
            rgb_key_color(i, rgb32(r, g, g, false));
        }
        hid_joy.axis ^= 0x80808080; // some magic number from CrazyRedMachine
        for (int i = 0; i < air_num(); i++) {
            uint8_t v = air_value(i) << 2;
            rgb_set_color(31 + i, rgb32(v, v, v, false));
        }

        slider_update_baseline();
        fps_count();
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
    printf("Get from USB %d-%d\n", report_id, report_type);
    return 0;
}

// Invoked when received SET_REPORT control request or
// received data on OUT endpoint ( Report ID = 0, Type = 0 )
void tud_hid_set_report_cb(uint8_t itf, uint8_t report_id,
                           hid_report_type_t report_type, uint8_t const *buffer,
                           uint16_t bufsize)
{
    if (report_type == HID_REPORT_TYPE_OUTPUT) {
        if (report_id == REPORT_ID_LED_SLIDER_15) {
            printf("Slider 16: %d\n", bufsize);
        } else if (report_id == REPORT_ID_LED_SLIDER_16) {
            printf("Slider 15: %d\n", bufsize);
        } else if (report_id == REPORT_ID_LED_TOWER_6) {
            printf("Tower 6: %d\n", bufsize);
        }
    }
}
