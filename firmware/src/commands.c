#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <ctype.h>

#include "pico/stdio.h"
#include "pico/stdlib.h"

#include "config.h"
#include "air.h"
#include "slider.h"
#include "save.h"
#include "cli.h"

#include "i2c_hub.h"

#include "nfc.h"
#include "aime.h"

#define SENSE_LIMIT_MAX 9
#define SENSE_LIMIT_MIN -9

static void disp_colors()
{
    printf("[Colors]\n");
    printf("  Key upper: %06lx, lower: %06lx, both: %06lx, off: %06lx\n", 
           chu_cfg->colors.key_on_upper, chu_cfg->colors.key_on_lower,
           chu_cfg->colors.key_on_both, chu_cfg->colors.key_off);
    printf("  Gap: %06lx\n", chu_cfg->colors.gap);
}

static void disp_style()
{
    printf("[Style]\n");
    printf("  Key: %d, Gap: %d, ToF: %d, Level: %d\n",
           chu_cfg->style.key, chu_cfg->style.gap,
           chu_cfg->style.tof, chu_cfg->style.level);
}

static void disp_tof()
{
    printf("[ToF]\n");
    printf("  Offset: %d, Pitch: %d\n", chu_cfg->tof.offset, chu_cfg->tof.pitch);
}

static void disp_ir()
{
    printf("[IR]\n");
    printf("  Enabled: %s\n", chu_cfg->ir.enabled ? "ON" : "OFF");
    printf("  Base:");
    for (int i = 0; i < count_of(chu_cfg->ir.base); i++) {
        printf(" %d", chu_cfg->ir.base[i]);
    }
    printf("\n  Trigger:");
    for (int i = 0; i < count_of(chu_cfg->ir.trigger); i++) {
        printf(" %d%%", chu_cfg->ir.trigger[i]);
    }
    printf("\n");
}

static void disp_sense()
{
    printf("[Sense]\n");
    printf("  Filter: %u, %u, %u\n", chu_cfg->sense.filter >> 6,
                                    (chu_cfg->sense.filter >> 4) & 0x03,
                                    chu_cfg->sense.filter & 0x07);
    printf("  Sensitivity (global: %+d):\n", chu_cfg->sense.global);
    printf("    | 1| 2| 3| 4| 5| 6| 7| 8| 9|10|11|12|13|14|15|16|\n");
    printf("  ---------------------------------------------------\n");
    printf("  A |");
    for (int i = 0; i < 16; i++) {
        printf("%+2d|", chu_cfg->sense.keys[i * 2]);
    }
    printf("\n  B |");
    for (int i = 0; i < 16; i++) {
        printf("%+2d|", chu_cfg->sense.keys[i * 2 + 1]);
    }
    printf("\n");
    printf("  Debounce (touch, release): %d, %d\n",
           chu_cfg->sense.debounce_touch, chu_cfg->sense.debounce_release);
}

static void disp_hid()
{
    printf("[HID]\n");
    printf("  Joy: %s, NKRO: %s.\n", 
           chu_cfg->hid.joy ? "on" : "off",
           chu_cfg->hid.nkro ? "on" : "off" );
}

static void disp_aime()
{
    printf("[AIME]\n");
    printf("   NFC Module: %s\n", nfc_module_name());
    printf("  Virtual AIC: %s\n", chu_cfg->aime.virtual_aic ? "ON" : "OFF");
    printf("         Mode: %d\n", chu_cfg->aime.mode);
}

static void disp_tweak()
{
    printf("[Tweak]\n");
    printf("  Skip Splitter LED: %s\n", chu_cfg->tweak.skip_split_led ? "ON" : "OFF");
}

void handle_display(int argc, char *argv[])
{
    const char *usage = "Usage: display [colors|style|tof|ir|sense|hid|aime|tweak]\n";
    if (argc > 1) {
        printf(usage);
        return;
    }

    if (argc == 0) {
        disp_colors();
        disp_style();
        disp_tof();
        disp_ir();
        disp_sense();
        disp_hid();
        disp_aime();
        disp_tweak();
        return;
    }

    const char *choices[] = {"colors", "style", "tof", "ir", "sense", "hid", "aime", "tweak"};
    switch (cli_match_prefix(choices, count_of(choices), argv[0])) {
        case 0:
            disp_colors();
            break;
        case 1:
            disp_style();
            break;
        case 2:
            disp_tof();
            break;
        case 3:
            disp_ir();
            break;
        case 4:
            disp_sense();
            break;
        case 5:
            disp_hid();
            break;
        case 6:
            disp_aime();
            break;
        case 7:
            disp_tweak();
            break;
        default:
            printf(usage);
            break;
    }
}

static void handle_level(int argc, char *argv[])
{
    const char *usage = "Usage: level <0..255>\n";
    if (argc != 1) {
        printf(usage);
        return;
    }

    int level = cli_extract_non_neg_int(argv[0], 0);
    if ((level < 0) || (level > 255)) {
        printf(usage);
        return;
    }

    chu_cfg->style.level = level;
    config_changed();
    disp_style();
}

static void handle_stat(int argc, char *argv[])
{
    if (argc == 0) {
        for (int col = 0; col < 4; col++) {
            printf(" %2dA |", col * 4 + 1);
            for (int i = 0; i < 4; i++) {
                printf("%6u|", slider_count(col * 8 + i * 2));
            }
            printf("\n   B |");
            for (int i = 0; i < 4; i++) {
                printf("%6u|", slider_count(col * 8 + i * 2 + 1));
            }
            printf("\n");
        }
    } else if ((argc == 1) &&
               (strncasecmp(argv[0], "reset", strlen(argv[0])) == 0)) {
        slider_reset_stat();
    } else {
        printf("Usage: stat [reset]\n");
    }
}

static void handle_hid(int argc, char *argv[])
{
    const char *usage = "Usage: hid <joy|nkro|both>\n";
    if (argc != 1) {
        printf(usage);
        return;
    }

    const char *choices[] = {"joy", "nkro", "both"};
    int match = cli_match_prefix(choices, 3, argv[0]);
    if (match < 0) {
        printf(usage);
        return;
    }

    chu_cfg->hid.joy = ((match == 0) || (match == 2)) ? 1 : 0;
    chu_cfg->hid.nkro = ((match == 1) || (match == 2)) ? 1 : 0;
    config_changed();
    disp_hid();
}

static void handle_tof(int argc, char *argv[])
{
    const char *usage = "Usage: tof <offset> [pitch]\n"
                        "  offset: 40..255\n"
                        "  pitch: 4..50\n";
    if (argc > 2) {
        printf(usage);
        return;
    }

    if (argc == 0) {
        printf("TOF: ");
        for (int i = air_tof_num(); i > 0; i--) {
            printf(" %4d", air_tof_raw(i - 1) / 10);
        }
        printf("\n");
        return;
    }

    int offset = chu_cfg->tof.offset;
    int pitch = chu_cfg->tof.pitch;
    if (argc >= 1) {
        offset = cli_extract_non_neg_int(argv[0], 0);
    }
    if (argc == 2) {
        pitch = cli_extract_non_neg_int(argv[1], 0);
    }

    if ((offset < 40) || (offset > 255) || (pitch < 4) || (pitch > 50)) {
        printf(usage);
        return;
    }

    chu_cfg->tof.offset = offset;
    chu_cfg->tof.pitch = pitch;

    config_changed();
    disp_tof();
}

static void air_diagnostic()
{
    chu_runtime.ir_diagnostics = !chu_runtime.ir_diagnostics;
    printf("IR Diagnostics: %s\n", chu_runtime.ir_diagnostics ? "ON" : "OFF");
}

static void air_baseline()
{
    printf("IR Baseline:");
    for (int i = 0; i < count_of(chu_cfg->ir.base); i++) {
        chu_cfg->ir.base[i] = air_ir_raw(i);
        printf(" %4d", chu_cfg->ir.base[i]);
    }
    printf("\n");
}

static void air_trigger(char *argv[])
{
    const char *usage = "Usage: ir trigger <percent>\n"
                        "  percent: [1..100]\n";

    if (strncasecmp(argv[0], "trigger", strlen(argv[0])) != 0) {
        printf(usage);
        return;
    }

    int percent = cli_extract_non_neg_int(argv[1], 0);
    if ((percent < 1) || (percent > 100)) {
        printf(usage);
        return;
    }

    for (int i = 0; i < count_of(chu_cfg->ir.trigger); i++) {
        chu_cfg->ir.trigger[i] = percent;
    }
    config_changed();
    disp_ir();
}

static void handle_ir(int argc, char *argv[])
{
    const char *usage = "Usage: ir <enable|disable|diagnostic|baseline>\n"
                        "       ir trigger <percent>\n"
                        "  percent: [1..100]\n";
    if (argc == 1) {
        const char *commands[] = { "enable", "disable", "diagnostic", "baseline" };
        int cmd = cli_match_prefix(commands, count_of(commands), argv[0]);
        if (cmd == 0) {
            chu_cfg->ir.enabled = true;
            air_init();
        } else if (cmd == 1) {
            chu_cfg->ir.enabled = false;
        } else if (cmd == 2) {
            air_diagnostic();
        } else if (cmd == 3) {
            air_baseline();
        } else {
            printf(usage);
            return;
        }
        config_changed();
    } else if (argc == 2) {
        air_trigger(argv);
    } else {
        printf(usage);
    }
}

static void handle_filter(int argc, char *argv[])
{
    const char *usage = "Usage: filter <first> <second> [interval]\n"
                        "Adjusts MPR121 noise filtering parameters (see datasheets).\n"
                        "    first:    First Filter Iterations  (FFI) [0..3]\n"
                        "    second:   Second Filter Iterations (SFI) [0..3]\n"
                        "    interval: Electrode Sample Interval (ESI) [0..7]\n";
    if ((argc < 2) || (argc > 3)) {
        printf(usage);
        return;
    }

    int ffi = cli_extract_non_neg_int(argv[0], 0);
    int sfi = cli_extract_non_neg_int(argv[1], 0);
    int intv = chu_cfg->sense.filter & 0x07;
    if (argc == 3) {
        intv = cli_extract_non_neg_int(argv[2], 0);
    }

    if ((ffi < 0) || (ffi > 3) || (sfi < 0) || (sfi > 3) ||
        (intv < 0) || (intv > 7)) {
        printf(usage);
        return;
    }
    chu_cfg->sense.filter = (ffi << 6) | (sfi << 4) | intv;
    slider_update_config();
    config_changed();
    disp_sense();
}

static int8_t *extract_key(const char *param)
{
    int len = strlen(param);

    int offset;
    if (toupper(param[len - 1]) == 'A') {
        offset = 0;
    } else if (toupper(param[len - 1]) == 'B') {
        offset = 1;
    } else {
        return NULL;
    }

    int id = cli_extract_non_neg_int(param, len - 1) - 1;
    if ((id < 0) || (id > 15)) {
        return NULL;
    }

    return &chu_cfg->sense.keys[id * 2 + offset];
}

static void sense_do_op(int8_t *target, char op)
{
    if (op == '+') {
        if (*target < SENSE_LIMIT_MAX) {
            (*target)++;
        }
    } else if (op == '-') {
        if (*target > SENSE_LIMIT_MIN) {
            (*target)--;
        }
    } else if (op == '0') {
        *target = 0;
    }
}

static void handle_sense(int argc, char *argv[])
{
    const char *usage = "Usage: sense [key|*] <+|-|0>\n"
                        "Example:\n"
                        "  >sense +\n"
                        "  >sense -\n"
                        "  >sense 1A +\n"
                        "  >sense 13B -\n"
                        "  >sense * 0\n";
    if ((argc < 1) || (argc > 2)) {
        printf(usage);
        return;
    }

    const char *op = argv[argc - 1];
    if ((strlen(op) != 1) || !strchr("+-0", op[0])) {
        printf(usage);
        return;
    }

    if (argc == 1) {
        sense_do_op(&chu_cfg->sense.global, op[0]);
    } else {
        if (strcmp(argv[0], "*") == 0) {
            for (int i = 0; i < 32; i++) {
                sense_do_op(&chu_cfg->sense.keys[i], op[0]);
            }
        } else {
            int8_t *key = extract_key(argv[0]);
            if (!key) {
                printf(usage);
                return;
            }
            sense_do_op(key, op[0]);
        }
    }

    slider_update_config();
    config_changed();
    disp_sense();
}

static void handle_debounce(int argc, char *argv[])
{
    const char *usage = "Usage: debounce <touch> [release]\n"
                        "  touch, release: 0..7\n";
    if ((argc < 1) || (argc > 2)) {
        printf(usage);
        return;
    }

    int touch = chu_cfg->sense.debounce_touch;
    int release = chu_cfg->sense.debounce_release;
    if (argc >= 1) {
        touch = cli_extract_non_neg_int(argv[0], 0);
    }
    if (argc == 2) {
        release = cli_extract_non_neg_int(argv[1], 0);
    }

    if ((touch < 0) || (release < 0) ||
        (touch > 7) || (release > 7)) {
        printf(usage);
        return;
    }

    chu_cfg->sense.debounce_touch = touch;
    chu_cfg->sense.debounce_release = release;

    slider_update_config();
    config_changed();
    disp_sense();
}

static void handle_raw()
{
    printf("%s\n", slider_sensor_status());
    printf("Raw readings:\n");
    const uint16_t *raw = slider_raw();
    printf("|");
    for (int i = 0; i < 16; i++) {
        printf("%3d|", raw[i * 2]);
    }
    printf("\n|");
    for (int i = 0; i < 16; i++) {
        printf("%3d|", raw[i * 2 + 1]);
    }
    printf("\n");
}

static void handle_save()
{
    save_request(true);
}

static void handle_factory_reset()
{
    config_factory_reset();
    printf("Factory reset done.\n");
}

static void handle_nfc()
{
    i2c_select(I2C_PORT, 1 << 5); // PN532 on IR1 (I2C mux chn 5)
    printf("NFC module: %s\n", nfc_module_name());
    nfc_rf_field(true);
    nfc_card_t card = nfc_detect_card();
    nfc_rf_field(false);
    printf("Card: %s", nfc_card_name(card.card_type));
    for (int i = 0; i < card.len; i++) {
        printf(" %02x", card.uid[i]);
    }
    printf("\n");
}

static bool handle_aime_mode(const char *mode)
{
    if (strcmp(mode, "0") == 0) {
        chu_cfg->aime.mode = 0;
    } else if (strcmp(mode, "1") == 0) {
        chu_cfg->aime.mode = 1;
    } else {
        return false;
    }
    aime_set_mode(chu_cfg->aime.mode);
    config_changed();
    return true;
}

static bool handle_aime_virtual(const char *onoff)
{
    if (strcasecmp(onoff, "on") == 0) {
        chu_cfg->aime.virtual_aic = 1;
    } else if (strcasecmp(onoff, "off") == 0) {
        chu_cfg->aime.virtual_aic = 0;
    } else {
        return false;
    }
    aime_virtual_aic(chu_cfg->aime.virtual_aic);
    config_changed();
    return true;
}

static void handle_aime(int argc, char *argv[])
{
    const char *usage = "Usage:\n"
                        "    aime mode <0|1>\n"
                        "    aime virtual <on|off>\n";
    if (argc != 2) {
        printf("%s", usage);
        return;
    }

    const char *commands[] = { "mode", "virtual" };
    int match = cli_match_prefix(commands, 2, argv[0]);
    
    bool ok = false;
    if (match == 0) {
        ok = handle_aime_mode(argv[1]);
    } else if (match == 1) {
        ok = handle_aime_virtual(argv[1]);
    }

    if (ok) {
        disp_aime();
    } else {
        printf("%s", usage);
    }
}

static void handle_tweak(int argc, char *argv[])
{
    const char *usage = "Usage: tweak skip_split <on|off>\n";
    if (argc < 1) {
        printf("%s", usage);
        return;
    }
    const char *options[] = { "skip_split" };
    int match = cli_match_prefix(options, count_of(options), argv[0]);
    if (match < 0) {
        printf("%s", usage);
        return;
    }

    if (match == 0) {
        if (argc != 2) {
            printf("%s", usage);
            return;
        }
        const char *on_off[] = { "off", "on" };
        int on = cli_match_prefix(on_off, count_of(on_off), argv[1]);
        if (on < 0) {
            printf("%s", usage);
            return;
        }
        chu_cfg->tweak.skip_split_led = (on > 0);
    }
    config_changed();
}

void commands_init()
{
    cli_register("display", handle_display, "Display all config.");
    cli_register("level", handle_level, "Set LED brightness level.");
    cli_register("stat", handle_stat, "Display or reset statistics.");
    cli_register("hid", handle_hid, "Set HID mode.");
    cli_register("tof", handle_tof, "Set ToF config.");
    cli_register("ir", handle_ir, "Set IR config.");
    cli_register("filter", handle_filter, "Set pre-filter config.");
    cli_register("sense", handle_sense, "Set sensitivity config.");
    cli_register("debounce", handle_debounce, "Set debounce config.");
    cli_register("raw", handle_raw, "Show key raw readings.");
    cli_register("tweak", handle_tweak, "Tweak options.");
    cli_register("save", handle_save, "Save config to flash.");
    cli_register("factory", handle_factory_reset, "Reset everything to default.");
    cli_register("nfc", handle_nfc, "NFC debug.");
    cli_register("aime", handle_aime, "AIME settings.");
}
