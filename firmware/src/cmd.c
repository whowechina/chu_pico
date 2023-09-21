#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <ctype.h>

#include "pico/stdio.h"
#include "pico/stdlib.h"

#include "config.h"
#include "slider.h"
#include "save.h"

#define SENSE_LIMIT_MAX 8
#define SENSE_LIMIT_MIN -8

#define MAX_COMMANDS 20
#define MAX_COMMAND_LENGTH 20
#define MAX_PARAMETERS 5
#define MAX_PARAMETER_LENGTH 20

const char *chu_prompt = "chu_pico>";

typedef void (*cmd_handler_t)(int argc, char *argv[]);

typedef struct {
    char name[MAX_COMMAND_LENGTH];
    cmd_handler_t handler;
} command_t;

command_t commands[MAX_COMMANDS];
int num_commands = 0;

static void register_command(char *name, cmd_handler_t handler)
{
    if (num_commands < MAX_COMMANDS) {
        strcpy(commands[num_commands].name, name);
        commands[num_commands].handler = handler;
        num_commands++;
    }
}

static void handle_help(int argc, char *argv[])
{
    printf("Available commands:\n");
    for (int i = 0; i < num_commands; i++) {
        printf("%s\n", commands[i].name);
    }
}

static void list_colors()
{
    printf("[Colors]\n");
    printf("  Key upper: %06x, lower: %06x, both: %06x, off: %06x\n", 
           chu_cfg->colors.key_on_upper, chu_cfg->colors.key_on_lower,
           chu_cfg->colors.key_on_both, chu_cfg->colors.key_off);
    printf("  Gap: %06x\n", chu_cfg->colors.gap);
}

static void list_style()
{
    printf("[Style]\n");
    printf("  Key: %d, Gap: %d, ToF: %d, Level: %d\n",
           chu_cfg->style.key, chu_cfg->style.gap,
           chu_cfg->style.tof, chu_cfg->style.level);
}

static void list_tof()
{
    printf("[ToF]\n");
    printf("  Offset: %d, Pitch: %d\n", chu_cfg->tof.offset, chu_cfg->tof.pitch);
}

static void list_sense()
{
    printf("[Sense]\n");
    printf("  Global: %d, debounce (touch, release): %d, %d\n", chu_cfg->sense.global,
           chu_cfg->sense.debounce_touch, chu_cfg->sense.debounce_release);
    printf("    | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10| 11| 12| 13| 14| 15| 16|\n");
    printf("    -----------------------------------------------------------------\n");
    printf("  A |");
    for (int i = 0; i < 16; i++) {
        printf("%3d|", chu_cfg->sense.keys[i * 2]);
    }
    printf("\n  B |");
    for (int i = 0; i < 16; i++) {
        printf("%3d|", chu_cfg->sense.keys[i * 2 + 1]);
    }
    printf("\n");
}

static void list_hid()
{
    printf("[HID]\n");
    printf("  Joy: %s, NKRO: %s.\n", 
           chu_cfg->hid.joy ? "on" : "off",
           chu_cfg->hid.nkro ? "on" : "off" );
}

void handle_list(int argc, char *argv[])
{
    const char *usage = "Usage: list [colors|style|tof|sense|hid]\n";
    if (argc > 1) {
        printf(usage);
        return;
    }

    if (argc == 0) {
        list_colors();
        list_style();
        list_tof();
        list_sense();
        list_hid();
        return;
    }

    if (strcmp(argv[0], "colors") == 0) {
        list_colors();
    } else if (strcmp(argv[0], "style") == 0) {
        list_style();
    } else if (strcmp(argv[0], "tof") == 0) {
        list_tof();
    } else if (strcmp(argv[0], "sense") == 0) {
        list_sense();
    } else if (strcmp(argv[0], "hid") == 0) {
        list_hid();
    } else {
        printf(usage);
    }
}

static int fps[2];
void fps_count(int core)
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

static void handle_fps(int argc, char *argv[])
{
    printf("FPS: core 0: %d, core 1: %d\n", fps[0], fps[1]);
}

static void handle_hid(int argc, char *argv[])
{
    const char *usage = "Usage: hid <joy|nkro>\n";
    if (argc != 1) {
        printf(usage);
        return;
    }

    if (strcmp(argv[0], "joy") == 0) {
        chu_cfg->hid.joy = 1;
        chu_cfg->hid.nkro = 0;
    } else if (strcmp(argv[0], "nkro") == 0) {
        chu_cfg->hid.joy = 0;
        chu_cfg->hid.nkro = 1;
    } else if (strcmp(argv[0], "both") == 0) {
        chu_cfg->hid.joy = 1;
        chu_cfg->hid.nkro = 1;
    } else {
        printf(usage);
        return;
    }
    config_changed();
}

static int extract_non_neg_int(const char *param, int len)
{
    if (len == 0) {
        len = strlen(param);
    }
    int result = 0;
    for (int i = 0; i < len; i++) {
        if (!isdigit(param[i])) {
            return -1;
        }
        result = result * 10 + param[i] - '0';
    }
    return result;
}

static void handle_tof(int argc, char *argv[])
{
    const char *usage = "Usage: tof <offset> [pitch]\n"
                        "  offset: 40-255\n"
                        "  pitch: 4-50\n";
    if ((argc < 1) || (argc > 2)) {
        printf(usage);
        return;
    }

    int offset = chu_cfg->tof.offset;
    int pitch = chu_cfg->tof.pitch;
    if (argc >= 1) {
        offset = extract_non_neg_int(argv[0], 0);
    }
    if (argc == 2) {
        pitch = extract_non_neg_int(argv[1], 0);
    }

    if ((offset < 40) || (offset > 255) || (pitch < 4) || (pitch > 50)) {
        printf(usage);
        return;
    }

    chu_cfg->tof.offset = offset;
    chu_cfg->tof.pitch = pitch;

    config_changed();
    list_tof();
}

static uint8_t *extract_key(const char *param)
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

    int id = extract_non_neg_int(param, len - 1) - 1;
    if ((id < 0) || (id > 15)) {
        return NULL;
    }

    return &chu_cfg->sense.keys[id * 2 + offset];
}

static void handle_sense(int argc, char *argv[])
{
    const char *usage = "Usage: sense [key] <+|->\n"
                        "Example:\n"
                        "  >sense +\n"
                        "  >sense -\n"
                        "  >sense 1A +\n"
                        "  >sense 13B -\n";
    if ((argc < 1) || (argc > 2)) {
        printf(usage);
        return;
    }

    int8_t *target = &chu_cfg->sense.global;
    const char *op = argv[argc - 1];
    if (argc == 2) {
        target = extract_key(argv[0]);
        if (!target) {
            printf(usage);
            return;
        }
    }

    if (strcmp(op, "+") == 0) {
        if (*target < SENSE_LIMIT_MAX) {
            (*target)++;
        }
    } else if (strcmp(op, "-") == 0) {
        if (*target > SENSE_LIMIT_MIN) {
            (*target)--;
        }
    } else {
        printf(usage);
        return;
    }

    config_changed();
    list_sense();
}

static void handle_debounce(int argc, char *argv[])
{
    const char *usage = "Usage: debounce <touch> [release]\n"
                        "  touch, release: 0-255\n";
    if ((argc < 1) || (argc > 2)) {
        printf(usage);
        return;
    }

    int touch = chu_cfg->sense.debounce_touch;
    int release = chu_cfg->sense.debounce_release;
    if (argc >= 1) {
        touch = extract_non_neg_int(argv[0], 0);
    }
    if (argc == 2) {
        release = extract_non_neg_int(argv[1], 0);
    }

    if ((touch < 0) || (release < 0)) {
        printf(usage);
        return;
    }

    chu_cfg->sense.debounce_touch = touch;
    chu_cfg->sense.debounce_release = release;

    config_changed();
    list_sense();
}

static void handle_baseline()
{
    printf("    | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10| 11| 12| 13| 14| 15| 16|\n");
    printf("    -----------------------------------------------------------------\n");
    printf("  A |");
    for (int i = 0; i < 16; i++) {
        printf("%3d|", slider_baseline(i * 2));
    }
    printf("\n  B |");
    for (int i = 0; i < 16; i++) {
        printf("%3d|", slider_baseline(i * 2 + 1));
    }
    printf("\n");
    printf(" dA |");
    for (int i = 0; i < 16; i++) {
        printf("%3d|", slider_delta(i * 2));
    }
    printf("\n dB |");
    for (int i = 0; i < 16; i++) {
        printf("%3d|", slider_delta(i * 2 + 1));
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

void cmd_init()
{
    register_command("?", handle_help);
    register_command("list", handle_list);
    register_command("fps", handle_fps);
    register_command("hid", handle_hid);
    register_command("tof", handle_tof);
    register_command("sense", handle_sense);
    register_command("debounce", handle_debounce);
    register_command("baseline", handle_baseline);
    register_command("save", handle_save);
    register_command("factory", config_factory_reset);
}

static char cmd_buf[256];
static int cmd_len = 0;

static void process_cmd()
{
    char *command;
    char *argv[MAX_PARAMETERS];
    int argc;

    command = strtok(cmd_buf, " \n");
    argc = 0;
    while ((argc < MAX_PARAMETERS) &&
           (argv[argc] = strtok(NULL, " \n")) != NULL) {
        argc++;
    }

    for (int i = 0; i < num_commands; i++) {
        if (strcmp(commands[i].name, command) == 0) {
            commands[i].handler(argc, argv);
            printf("\n");
            break;
        }
    }
}

void cmd_run()
{
    int c = getchar_timeout_us(0);
    if (c == EOF) {
        return;
    }

    if ((c != '\n') && (c != '\r')) {
        if (cmd_len < sizeof(cmd_buf) - 2) {
            cmd_buf[cmd_len] = c;
            printf("%c", c);
            cmd_len++;
        }
        return;
    }

    cmd_buf[cmd_len] = '\0';
    cmd_len = 0;

    printf("\n");

    process_cmd();

    printf(chu_prompt);
}
