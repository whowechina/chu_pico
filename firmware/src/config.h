/*
 * Controller Config
 * WHowe <github.com/whowechina>
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>
#include <stdbool.h>

typedef struct __attribute__((packed)) {
    struct {
        uint32_t key_on_upper;
        uint32_t key_on_lower;
        uint32_t key_on_both;
        uint32_t key_off;
        uint32_t gap;
    } colors;
    struct {
        uint8_t key;
        uint8_t gap;
        uint8_t tof;
        uint8_t level;
    } style;
    struct {
        uint8_t offset;
        uint8_t pitch;
    } tof;
    struct {
        uint8_t filter; // FFI[6..7], SFI[4..5], ESI[0..3]
        int8_t global;
        uint8_t debounce_touch;
        uint8_t debounce_release;        
        int8_t keys[32];
    } sense;
    struct {
        uint8_t joy : 4;
        uint8_t nkro : 4;
    } hid;
    struct {
        uint8_t mode : 4;
        uint8_t virtual_aic : 4;
    } aime;
    struct {
        bool enabled;
        uint16_t base[6];
        uint8_t trigger[6];
    } ir;
    struct {
        bool skip_split_led;
        uint8_t reserved[7];
    } tweak;
} chu_cfg_t;

typedef struct {
    bool debug;
    bool ir_diagnostics;
} chu_runtime_t;

extern chu_cfg_t *chu_cfg;
extern chu_runtime_t chu_runtime;

void config_init();
void config_changed(); // Notify the config has changed
void config_factory_reset(); // Reset the config to factory default

#endif
