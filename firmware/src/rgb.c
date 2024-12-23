/*
 * RGB LED (WS2812) Strip control
 * WHowe <github.com/whowechina>
 * 
 */

#include "rgb.h"

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "hardware/pio.h"
#include "hardware/timer.h"

#include "ws2812.pio.h"

#include "board_defs.h"
#include "config.h"

static uint32_t buf_main[47]; // 16(Keys) + 15(Gaps) + 16 (ToF/Tower indicators)
static const uint32_t *buf_tower = &buf_main[31];

#define _MAP_LED(x) _MAKE_MAPPER(x)
#define _MAKE_MAPPER(x) MAP_LED_##x
#define MAP_LED_RGB { c1 = r; c2 = g; c3 = b; }
#define MAP_LED_GRB { c1 = g; c2 = r; c3 = b; }

#define REMAP_BUTTON_RGB _MAP_LED(BUTTON_RGB_ORDER)
#define REMAP_TT_RGB _MAP_LED(TT_RGB_ORDER)

static inline uint32_t _rgb32(uint32_t c1, uint32_t c2, uint32_t c3, bool gamma_fix)
{
    if (gamma_fix) {
        c1 = ((c1 + 1) * (c1 + 1) - 1) >> 8;
        c2 = ((c2 + 1) * (c2 + 1) - 1) >> 8;
        c3 = ((c3 + 1) * (c3 + 1) - 1) >> 8;
    }
    
    return (c1 << 16) | (c2 << 8) | (c3 << 0);    
}

uint32_t rgb32(uint32_t r, uint32_t g, uint32_t b, bool gamma_fix)
{
#if BUTTON_RGB_ORDER == GRB
    return _rgb32(g, r, b, gamma_fix);
#else
    return _rgb32(r, g, b, gamma_fix);
#endif
}

uint32_t rgb32_from_hsv(uint8_t h, uint8_t s, uint8_t v)
{
    uint32_t region, remainder, p, q, t;

    if (s == 0) {
        return v << 16 | v << 8 | v;
    }

    region = h / 43;
    remainder = (h % 43) * 6;

    p = (v * (255 - s)) >> 8;
    q = (v * (255 - ((s * remainder) >> 8))) >> 8;
    t = (v * (255 - ((s * (255 - remainder)) >> 8))) >> 8;

    switch (region) {
        case 0:
            return v << 16 | t << 8 | p;
        case 1:
            return q << 16 | v << 8 | p;
        case 2:
            return p << 16 | v << 8 | t;
        case 3:
            return p << 16 | q << 8 | v;
        case 4:
            return t << 16 | p << 8 | v;
        default:
            return v << 16 | p << 8 | q;
    }
}

static void drive_led()
{
    static uint64_t last = 0;
    uint64_t now = time_us_64();
    if (now - last < 4000) { // no faster than 250Hz
        return;
    }
    last = now;

    for (int i = 30; i >= 0; i--) {
        if (chu_cfg->tweak.skip_split_led && (i % 2 == 1)) {
            continue;
        }
        pio_sm_put_blocking(pio0, 0, buf_main[i] << 8u);
    }
    for (int i = 31; i < count_of(buf_main); i++) {
        pio_sm_put_blocking(pio0, 0, buf_main[i] << 8u);
    }

    for (int i = 0; i < 6; i++) {
        pio_sm_put_blocking(pio0, 0, buf_tower[i] << 8u);
    }
    for (int i = 0; i < 6; i++) {
        pio_sm_put_blocking(pio0, 1, buf_tower[i] << 8u);
    }
}

void rgb_set_colors(const uint32_t *colors, unsigned index, size_t num)
{
    if (index >= count_of(buf_main)) {
        return;
    }
    if (index + num > count_of(buf_main)) {
        num = count_of(buf_main) - index;
    }
    memcpy(&buf_main[index], colors, num * sizeof(*colors));
}

static inline uint32_t apply_level(uint32_t color)
{
    unsigned r = (color >> 16) & 0xff;
    unsigned g = (color >> 8) & 0xff;
    unsigned b = color & 0xff;

    r = r * chu_cfg->style.level / 255;
    g = g * chu_cfg->style.level / 255;
    b = b * chu_cfg->style.level / 255;

    return r << 16 | g << 8 | b;
}

void rgb_set_color(unsigned index, uint32_t color)
{
    if (index >= count_of(buf_main)) {
        return;
    }
    buf_main[index] = apply_level(color);
}

void rgb_key_color(unsigned index, uint32_t color)
{
    if (index > 16) {
        return;
    }
    buf_main[index * 2] = apply_level(color);
}

void rgb_gap_color(unsigned index, uint32_t color)
{
    if (index > 15) {
        return;
    }
    buf_main[index * 2 + 1] = apply_level(color);
}

void rgb_set_brg(unsigned index, const uint8_t *brg_array, size_t num)
{
    if (index >= count_of(buf_main)) {
        return;
    }
    if (index + num > count_of(buf_main)) {
        num = count_of(buf_main) - index;
    }
    for (int i = 0; i < num; i++) {
        uint8_t b = brg_array[i * 3 + 0];
        uint8_t r = brg_array[i * 3 + 1];
        uint8_t g = brg_array[i * 3 + 2];
        buf_main[index + i] = apply_level(rgb32(r, g, b, false));
    }
}

void rgb_init()
{
    uint offset = pio_add_program(pio0, &ws2812_program);
    ws2812_program_init(pio0, 0, offset, RGB_MAIN_PIN, 800000, false);
    ws2812_program_init(pio0, 1, offset, RGB_TOWER_LEFT_PIN, 800000, false);
    ws2812_program_init(pio0, 2, offset, RGB_TOWER_RIGHT_PIN, 800000, false);
}

void rgb_update()
{
    drive_led();
}
