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
        .level = 7,
    },
    .tof = {
        .offset = 64,
        .pitch = 16,
    },
    .sense = {
        .debounce = 4,
     },
    .hid = {
        .joy = 0x0f,
        .nkro = 0,
    },
};

chu_runtime_t *chu_runtime;

static void config_loaded()
{
    if (chu_cfg->style.level > 10) {
        chu_cfg->style.level = default_cfg.style.level;
        config_changed();
    }
    if ((chu_cfg->tof.offset < 400) || (chu_cfg->tof.offset > 2000) ||
        (chu_cfg->tof.pitch < 50) || (chu_cfg->tof.pitch > 500)) {
        chu_cfg->tof = default_cfg.tof;
        config_changed();
    }

    if (chu_cfg->sense.debounce > 32) {
        chu_cfg->sense.debounce = default_cfg.sense.debounce;
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
