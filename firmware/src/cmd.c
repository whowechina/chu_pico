#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

#include "pico/stdio.h"
#include "pico/stdlib.h"

#include "config.h"

#define MAX_COMMANDS 10
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

void register_command(char *name, cmd_handler_t handler)
{
    if (num_commands < MAX_COMMANDS) {
        strcpy(commands[num_commands].name, name);
        commands[num_commands].handler = handler;
        num_commands++;
    }
}

void handle_help(int argc, char *argv[])
{
    printf("Available commands:\n");
    for (int i = 0; i < num_commands; i++) {
        printf("%s\n", commands[i].name);
    }
}

void handle_list(int argc, char *argv[])
{
    printf("[Colors]\n");
    printf("  Key upper: %6x, lower: %6x, both: %6x, off: %6x\n", 
           chu_cfg->colors.key_on_upper, chu_cfg->colors.key_on_lower,
           chu_cfg->colors.key_on_both, chu_cfg->colors.key_off);
    printf("  Gap: %6x\n", chu_cfg->colors.gap);
    printf("[Style]\n");
    printf("  Key: %d, Gap: %d, ToF: %d, Level: %d\n", 
           chu_cfg->style.key, chu_cfg->style.gap,
           chu_cfg->style.tof, chu_cfg->style.level);
    printf("[ToF]\n");
    printf("  Offset: %d, Pitch: %d\n", chu_cfg->tof.offset, chu_cfg->tof.pitch);
    printf("[Sense]\n");
    printf("  Global: %d, debounce: %d\n", chu_cfg->sense.global, chu_cfg->sense.debounce);
    printf("    | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | A | B | C | D | E | F |\n");
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

    printf("[HID]\n");
    printf("  Joy: %s, NKRO: %s.\n", 
           chu_cfg->hid.joy ? "on" : "off",
           chu_cfg->hid.nkro ? "on" : "off" );
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

void handle_fps(int argc, char *argv[])
{
    printf("FPS: core 0: %d, core 1: %d\n", fps[0], fps[1]);
}

void handle_hid(int argc, char *argv[])
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

void cmd_init()
{
    fcntl(0, F_SETFL, O_NONBLOCK);
    register_command("?", handle_help);
    register_command("list", handle_list);
    register_command("fps", handle_fps);
    register_command("hid", handle_hid);
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
