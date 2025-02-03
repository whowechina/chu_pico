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

#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/sync.h"
#include "hardware/structs/ioqspi.h"
#include "hardware/structs/sio.h"

#include "tusb.h"
#include "usb_descriptors.h"

#include "aime.h"
#include "nfc.h"

#include "board_defs.h"
#include "save.h"
#include "config.h"
#include "cli.h"
#include "commands.h"

#include "i2c_hub.h"
#include "slider.h"
#include "air.h"
#include "rgb.h"
#include "button.h"
#include "lzfx.h"

struct __attribute__((packed)) {
    uint16_t buttons; // 16 buttons; see JoystickButtons_t for bit mapping
    uint8_t  HAT;    // HAT switch; one nibble w/ unused nibble
    uint32_t axis;  // slider touch data
    uint8_t  VendorSpec;
} hid_joy, sent_hid_joy;

struct __attribute__((packed)) {
    uint8_t modifier;
    uint8_t keymap[15];
} hid_nkro, sent_hid_nkro;

void report_usb_hid()
{
    if (tud_hid_ready()) {
        hid_joy.HAT = 0;
        hid_joy.VendorSpec = 0;
        if ((chu_cfg->hid.joy) &&
            (memcmp(&hid_joy, &sent_hid_joy, sizeof(hid_joy)) != 0)) {
            if (tud_hid_report(REPORT_ID_JOYSTICK, &hid_joy, sizeof(hid_joy))) {
                sent_hid_joy = hid_joy;
            }
        }
        if (chu_cfg->hid.nkro &&
            (memcmp(&hid_nkro, &sent_hid_nkro, sizeof(hid_nkro)) != 0)) {
            if (tud_hid_n_report(0x02, 0, &sent_hid_nkro, sizeof(sent_hid_nkro))) {
                sent_hid_nkro = hid_nkro;
            }        
        }
    }
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
    uint16_t airmap = air_bitmap();

    /* to cope with Redboard mapping which I don't really understand why */
    hid_joy.buttons = ((airmap >> 1) & 0x07) | ((airmap & 0x01) << 3) | (airmap & 0x30);

    uint16_t aux = button_read();
    hid_joy.buttons |= (aux & 0x01) ? 0x200 : 0; // START
    hid_joy.buttons |= (aux & 0x02) ? 0x100 : 0; // SERVICE
    hid_joy.buttons |= (aux & 0x04) ? 0x1000 : 0; // TEST
}

const uint8_t keycode_table[128][2] = { HID_ASCII_TO_KEYCODE };
const uint8_t keymap[41 + 1] = NKRO_KEYMAP; // 32 keys, 6 air keys, 3 aux, 1 terminator
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

    uint16_t airmap = air_bitmap();
    for (int i = 0; i < 6; i++) {
        uint8_t code = keycode_table[keymap[32 + i]][1];
        uint8_t byte = code / 8;
        uint8_t bit = code % 8;
        if (airmap & (1 << i)) {
            hid_nkro.keymap[byte] |= (1 << bit);
        } else {
            hid_nkro.keymap[byte] &= ~(1 << bit);
        }
    }

    uint16_t aux = button_read();
    for (int i = 0; i < 3; i++) {
        uint8_t code = keycode_table[keymap[38 + i]][1];
        uint8_t byte = code / 8;
        uint8_t bit = code % 8;
        if (aux & (1 << i)) {
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

    if (now - last_hid_time >= 1000000) {
        const uint32_t colors[] = {0x000000, 0x0000ff, 0xff0000, 0xffff00,
                                0x00ff00, 0x00ffff, 0xffffff};
        for (int i = 0; i < air_tof_num(); i++) {
            int d = air_tof_value(i);
            rgb_set_color(31 + i, colors[d]);
        }

        for (int i = 0; i < 15; i++) {
            uint32_t color = rgb32_from_hsv(i * 573 / 15, 255, 16);
            rgb_gap_color(i, color);
        }

        for (int i = 0; i < 16; i++) {
            bool r = slider_touched(i * 2);
            bool g = slider_touched(i * 2 + 1);
            rgb_set_color(30 - i * 2, rgb32(r ? 80 : 0, g ? 80 : 0, 0, false));
        }
    }

    uint32_t aime_color = aime_led_color();
    if (aime_color > 0) {
        uint8_t r = aime_color >> 16;
        uint8_t g = aime_color >> 8;
        uint8_t b = aime_color;

        uint32_t blink = now / 100000;
        aime_color = (blink & 1) ? rgb32(r, g, b, false) : 0;

        rgb_set_color(34, aime_color);
        rgb_set_color(35, aime_color);
    }
}

const int aime_intf = 1;
static void cdc_aime_putc(uint8_t byte)
{
    tud_cdc_n_write(aime_intf, &byte, 1);
    tud_cdc_n_write_flush(aime_intf);
}

static void aime_run()
{
    if (tud_cdc_n_available(aime_intf)) {
        uint8_t buf[32];
        uint32_t count = tud_cdc_n_read(aime_intf, buf, sizeof(buf));

        i2c_select(I2C_PORT, 1 << 5); // PN532 on IR1 (I2C mux chn 5)
        for (int i = 0; i < count; i++) {
            aime_feed(buf[i]);
        }
    }
}

static void runtime_ctrl()
{
    /* Just use long-press SERVICE to reset touch in runtime */
    static bool applied = false;
    static uint64_t press_time = 0;
    static bool last_svc_button = false;
    bool svc_button = button_read() & 0x02;

    if (svc_button) {
        if (!last_svc_button) {
            press_time = time_us_64();
            applied = false;
        }
        if (!applied && (time_us_64() - press_time > 2000000)) {
            slider_sensor_init();
            applied = true;
        }
    }

    last_svc_button = svc_button;
}

static mutex_t core1_io_lock;
static void core1_loop()
{
    while (1) {
        if (mutex_try_enter(&core1_io_lock, NULL)) {
            run_lights();
            rgb_update();
            mutex_exit(&core1_io_lock);
        }
        cli_fps_count(1);
        sleep_ms(1);
    }
}

static void core0_loop()
{
    uint64_t next_frame = time_us_64();
    while(1) {
        tud_task();

        cli_run();
        aime_run();
    
        save_loop();
        cli_fps_count(0);

        slider_update();
        air_update();
        button_update();

        gen_joy_report();
        gen_nkro_report();
        report_usb_hid();

        runtime_ctrl();

        sleep_until(next_frame);
        next_frame += 1000;
    }
}

/* if certain key pressed when booting, enter update mode */
static void update_check()
{
    const uint8_t pins[] = BUTTON_DEF;
    int pressed = 0;
    for (int i = 0; i < count_of(pins); i++) {
        uint8_t gpio = pins[i];
        gpio_init(gpio);
        gpio_set_function(gpio, GPIO_FUNC_SIO);
        gpio_set_dir(gpio, GPIO_IN);
        gpio_pull_up(gpio);
        sleep_ms(1);
        if (!gpio_get(gpio)) {
            pressed++;
        }
    }

    if (pressed >= 2) {
        sleep_ms(100);
        reset_usb_boot(0, 2);
        return;
    }
}

void init()
{
    sleep_ms(50);
    set_sys_clock_khz(150000, true);
    board_init();

    update_check();

    tusb_init();
    stdio_init_all();

    config_init();
    mutex_init(&core1_io_lock);
    save_init(0xca34cafe, &core1_io_lock);

    button_init();
    slider_init();
    air_init();
    rgb_init();

    nfc_attach_i2c(I2C_PORT);
    i2c_select(I2C_PORT, 1 << 5); // PN532 on IR1 (I2C mux chn 5)
    nfc_init();
    aime_init(cdc_aime_putc);
    aime_virtual_aic(chu_cfg->aime.virtual_aic);
    aime_sub_mode(chu_cfg->aime.mode);

    cli_init("chu_pico>", "\n   << Chu Pico Controller >>\n"
                            " https://github.com/whowechina\n\n");
    
    commands_init();
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

            if (!chu_cfg->hid.joy) {
                chu_cfg->hid.joy = 1;
                config_changed();
            }
        }
        last_hid_time = time_us_64();
        return;
    }
}
