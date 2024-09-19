/*
 * Controller Config and Runtime Data
 * WHowe <github.com/whowechina>
 * 
 * Config is a global data structure that stores all the configuration
 * Runtime is something to share between files.
 */

#include "config.h"
#include "save.h"

chu_cfg_t *chu_cfg;

static chu_cfg_t default_cfg = {
    .colors = {
        .key_on_upper = 0x00FF00,
        .key_on_lower = 0xff0000,
        .key_on_both = 0xff0000,
        .key_off = 0x000000,
        .gap = 0x000000,
    },
    .style = {
        .key = 0,
        .gap = 0,
        .tof = 0,
        .level = 127,
    },
    .tof = {
        .offset = 80,
        .pitch = 20,
    },
    .sense = {
        .filter = 0x10,
        .debounce_touch = 1,
        .debounce_release = 2,
     },
    .hid = {
        .joy = 1,
        .nkro = 0,
    },
    .aime = {
        .mode = 0,
        .virtual_aic = 0,
    },
    .ir = {
        .enabled = 0,
        .base = { 3800, 3800, 3800, 3800, 3800, 3800 },
        .trigger = { 20, 20, 20, 20, 20, 20 },
    },
};

chu_runtime_t chu_runtime = {0};

static void config_loaded()
{
    if ((chu_cfg->tof.offset < 40) ||
        (chu_cfg->tof.pitch < 4) || (chu_cfg->tof.pitch > 50)) {
        chu_cfg->tof = default_cfg.tof;
        config_changed();
    }
    if ((chu_cfg->sense.filter & 0x0f) > 3 ||
        ((chu_cfg->sense.filter >> 4) & 0x0f) > 3) {
        chu_cfg->sense.filter = default_cfg.sense.filter;
        config_changed();
    }
    if ((chu_cfg->sense.global > 9) || (chu_cfg->sense.global < -9)) {
        chu_cfg->sense.global = default_cfg.sense.global;
        config_changed();
    }
    for (int i = 0; i < 32; i++) {
        if ((chu_cfg->sense.keys[i] > 9) || (chu_cfg->sense.keys[i] < -9)) {
            chu_cfg->sense.keys[i] = default_cfg.sense.keys[i];
            config_changed();
        }
    }
    if ((chu_cfg->sense.debounce_touch > 7) |
        (chu_cfg->sense.debounce_release > 7)) {
        chu_cfg->sense.debounce_touch = default_cfg.sense.debounce_touch;
        chu_cfg->sense.debounce_release = default_cfg.sense.debounce_release;
        config_changed();
    }
}

void config_changed()
{
    save_request(false);
}

void config_factory_reset()
{
    *chu_cfg = default_cfg;
    save_request(true);
}

void config_init()
{
    chu_cfg = (chu_cfg_t *)save_alloc(sizeof(*chu_cfg), &default_cfg, config_loaded);
}
