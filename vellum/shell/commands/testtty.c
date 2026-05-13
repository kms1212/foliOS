#include <vellum/shell.h>

#include <stdio.h>
#include <stdint.h>

static uint8_t clamp_rgb(int v)
{
    if (v < 0) {
        return 0;
    }
    if (v > 255) {
        return 255;
    }
    return v;
}

static uint8_t hue2rgb(int p, int q, int t)
{
    while (t < 0) {
        t += 1536;
    }
    while (t >= 1536) {
        t -= 1536;
    }

    if (t < 256) {
        return clamp_rgb(p + ((((q - p) * t) + 128) / 256));
    }
    if (t < 768) {
        return clamp_rgb(q);
    }
    if (t < 1024) {
        return clamp_rgb(p + ((((q - p) * (1024 - t)) + 128) / 256));
    }
    return clamp_rgb(p);
}

static void hsl2rgb(uint32_t h, uint8_t s, uint8_t l, uint8_t rgb[3])
{
    if (s == 0) {
        rgb[0] = rgb[1] = rgb[2] = l;
    } else {
        int q = l < 128 ? ((l * (255 + s)) + 127) / 255 : l + s - (((l * s) + 127) / 255);
        int p = (2 * l) - q;
        int t = h * 1536 / 63;

        rgb[0] = hue2rgb(p, q, t + 512);
        rgb[1] = hue2rgb(p, q, t);
        rgb[2] = hue2rgb(p, q, t - 512);
    }
}

static int testtty_handler(struct shell_instance *inst, int argc, char **argv)
{
    fputs("\x1b[0m", stdout);
    fputs("\x1b[1mbold\x1b[0m\n", stdout);
    fputs("\x1b[2mdim\x1b[0m\n", stdout);
    fputs("\x1b[3mitalic\x1b[0m\n", stdout);
    fputs("\x1b[4munderline\x1b[0m\n", stdout);
    fputs("\x1b[5mblink slow\x1b[0m\n", stdout);
    fputs("\x1b[6mblink fast\x1b[0m\n", stdout);
    fputs("\x1b[7mreversed\x1b[0m\n", stdout);
    fputs("\x1b[9mstrike\x1b[0m\n", stdout);
    fputs("\x1b[53moverline\x1b[0m\n", stdout);
    for (int i = 0; i < 8; i++) {
        printf("\x1b[%dm@", 30 + i);
    }
    for (int i = 0; i < 8; i++) {
        printf("\x1b[%dm@", 90 + i);
    }
    fputs("\x1b[0m\n", stdout);
    for (int i = 0; i < 16; i++) {
        printf("\x1b[38;5;%dm@", i);
    }
    fputs("\x1b[0m\n", stdout);
    for (int i = 16; i < 232; i++) {
        printf("\x1b[38;5;%dm@", i);
        if ((i - 16) % 36 == 35) {
            fputs("\x1b[0m\n", stdout);
        }
    }
    for (int i = 232; i < 256; i++) {
        printf("\x1b[38;5;%dm@", i);
    }
    fputs("\x1b[0m\n", stdout);
    for (int g = 0; g < 8; g++) {
        for (int b = 0; b < 8; b++) {
            for (int r = 0; r < 8; r++) {
                printf("\x1b[38;2;%d;%d;%dm@", r * 255 / 7, g * 255 / 7, b * 255 / 7);
            }
            fputs("\x1b[0m", stdout);
        }
        fputs("\x1b[0m\n", stdout);
    }
    for (int i = 0; i < 8; i++) {
        printf("\x1b[%dm ", 40 + i);
    }
    for (int i = 0; i < 8; i++) {
        printf("\x1b[%dm ", 100 + i);
    }
    fputs("\x1b[0m\n", stdout);
    for (int i = 0; i < 16; i++) {
        printf("\x1b[48;5;%dm ", i);
    }
    fputs("\x1b[0m\n", stdout);
    for (int i = 16; i < 232; i++) {
        printf("\x1b[48;5;%dm ", i);
        if ((i - 16) % 36 == 35) {
            fputs("\x1b[0m\n", stdout);
        }
    }
    for (int i = 232; i < 256; i++) {
        printf("\x1b[48;5;%dm ", i);
    }
    fputs("\x1b[0m\n", stdout);
    for (int g = 0; g < 8; g++) {
        for (int b = 0; b < 8; b++) {
            for (int r = 0; r < 8; r++) {
                printf("\x1b[48;2;%d;%d;%dm ", r * 255 / 7, g * 255 / 7, b * 255 / 7);
            }
            fputs("\x1b[0m", stdout);
        }
        fputs("\x1b[0m\n", stdout);
    }
    for (int l = 7; l >= 0; l--) {
        for (int h = 0; h < 64; h++) {
            uint8_t rgb[3];
            hsl2rgb(h, 255, l * 255 / 7, rgb);
            printf("\x1b[48;2;%d;%d;%dm ", rgb[0], rgb[1], rgb[2]);
        }
        fputs("\x1b[0m\n", stdout);
    }
    return 0;
}

static struct command testtty_command = {
    .name = "testtty",
    .handler = testtty_handler,
    .help_message = "Test TTY functions",
};

static void testtty_command_init(void)
{
    VlShell_RegisterCommand(&testtty_command);
}

REGISTER_SHELL_COMMAND(testtty, testtty_command_init)
