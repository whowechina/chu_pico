/*
 * Controller Main
 * WHowe <github.com/whowechina>
 */

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "pico/stdio.h"
#include "pico/stdlib.h"
#include "bsp/board.h"
#include "pico/multicore.h"
#include "pico/bootrom.h"

#include "hardware/gpio.h"
#include "hardware/sync.h"
#include "hardware/structs/ioqspi.h"
#include "hardware/structs/sio.h"

#include "tusb.h"
#include "usb_descriptors.h"

#include "board_defs.h"
#include "slider.h"
#include "air.h"
#include "rgb.h"
#include "lzfx.h"

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

struct __attribute__((packed)) {
    uint8_t modifier;
    uint8_t keymap[15];
} hid_nkro, sent_hid_nkro;

void report_usb_hid()
{
    if (tud_hid_ready()) {
        hid_joy.HAT = 0;
        hid_joy.VendorSpec = 0;
        tud_hid_n_report(0x00, REPORT_ID_JOYSTICK, &hid_joy, sizeof(hid_joy));
        if (memcmp(&hid_nkro, &sent_hid_nkro, sizeof(hid_nkro)) != 0) {
            sent_hid_nkro = hid_nkro;
            tud_hid_n_report(0x02, 0, &sent_hid_nkro, sizeof(sent_hid_nkro));
        }
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

static int fps[2] = {0};

static int get_fps(int core)
{
    return fps[core];
}

static void fps_count(int core)
{
    static uint32_t last[2] = {0};
    static int counter[2] = {0};

    counter[core]++;

    uint32_t now = time_us_32();
    if (now - last[core] < 1000000) {
        return;
    }
    last[core] = now;
    fps[core] = counter[core];
    counter[core] = 0;
}

static void print_fps()
{
    static uint32_t last = 0;
    uint32_t now = time_us_32();
    if (now - last < 5000000) {
        return;
    }
    last = now;
    printf("FPS: %d %d\n", get_fps(0), get_fps(1));
}

static void gen_joy_report()
{
    hid_joy.axis = 0;
    for (int i = 0; i < 16; i++) {
        if (slider_touched(i * 2)) {
            hid_joy.axis |= 1 << (30 - i * 2);
        }
        if (slider_touched(i * 2 + 1)) {
            hid_joy.axis |= 1 << (31 - i * 2);
        }

    }
    hid_joy.axis ^= 0x80808080; // some magic number from CrazyRedMachine
    hid_joy.buttons = air_bitmap();
}

const uint8_t keycode_table[128][2] = { HID_ASCII_TO_KEYCODE };
const char keymap[38 + 1] = NKRO_KEYMAP; // 32 keys, 6 air keys, 1 terminator
static void gen_nkro_report()
{
    for (int i = 0; i < 32; i++) {
        uint8_t code = keycode_table[keymap[i]][1];
        uint8_t byte = code / 8;
        uint8_t bit = code % 8;
        if (slider_touched(i)) {
            hid_nkro.keymap[byte] |= (1 << bit);
        } else {
            hid_nkro.keymap[byte] &= ~(1 << bit);
        }
    }
    for (int i = 0; i < 6; i++) {
        uint8_t code = keycode_table[keymap[32 + i]][1];
        uint8_t byte = code / 8;
        uint8_t bit = code % 8;
        if (hid_joy.buttons & (1 << i)) {
            hid_nkro.keymap[byte] |= (1 << bit);
        } else {
            hid_nkro.keymap[byte] &= ~(1 << bit);
        }
    }
}

static uint64_t last_hid_time = 0;

static void run_lights()
{
    uint64_t now = time_us_64();
    if (now - last_hid_time < 1000000) {
        return;
    }

    const uint32_t colors[] = {0x000000, 0x0000ff, 0xff0000, 0xffff00,
                               0x00ff00, 0x00ffff, 0xffffff};
    for (int i = 0; i < air_num(); i++) {
        int d = air_value(i);
        rgb_set_color(31 + i, colors[d]);
    }

    for (int i = 0; i < 15; i++) {
        int x = 15 - i;
        uint8_t r = (x & 0x01) ? 10 : 0;
        uint8_t g = (x & 0x02) ? 10 : 0;
        uint8_t b = (x & 0x04) ? 10 : 0;
        rgb_gap_color(i, rgb32(r, g, b, false));
    }

    for (int i = 0; i < 16; i++) {
        bool r = slider_touched(i * 2);
        bool g = slider_touched(i * 2 + 1);
        rgb_set_color(30 - i * 2, rgb32(r ? 80 : 0, g ? 80 : 0, 0, false));
    }
}

static void core1_loop()
{
    while (1) {
        rgb_update();
        fps_count(1);
        sleep_ms(1);
    }
}

static void core0_loop()
{
    while(1) {
        tud_task();
        slider_update();
        air_update();

        gen_joy_report();
        gen_nkro_report();
        report_usb_hid();

        run_lights();

        slider_update_baseline();
        fps_count(0);
        print_fps();
    }
}

void init()
{
    sleep_ms(200);
//    set_sys_clock_khz(166000, true);
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
        if (report_id == REPORT_ID_LED_SLIDER_16) {
            rgb_set_brg(0, buffer, bufsize / 3);
        } else if (report_id == REPORT_ID_LED_SLIDER_15) {
            rgb_set_brg(16, buffer, bufsize / 3);
        } else if (report_id == REPORT_ID_LED_TOWER_6) {
            rgb_set_brg(31, buffer, bufsize / 3);
        }
        last_hid_time = time_us_64();
        return;
    } 
    
    if (report_type == HID_REPORT_TYPE_FEATURE) {
        if (report_id == REPORT_ID_LED_COMPRESSED) {
            uint8_t buf[(48 + 45 + 6) * 3];
            unsigned int olen = sizeof(buf);
            if (lzfx_decompress(buffer + 1, buffer[0], buf, &olen) == 0) {
                rgb_set_brg(0, buf, olen / 3);
            }
        }
        last_hid_time = time_us_64();
        return;
    }
}
