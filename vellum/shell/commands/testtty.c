#include <vellum/shell.h>

#include <stdio.h>

static float hue2rgb(float p, float q, float t)
{
    if (t < 0.0f) t += 1.0f;
    if (t > 1.0f) t -= 1.0f;
    if (t < 1.0f / 6.0f) return p + (q - p) * 6.0f * t;
    if (t < 1.0f / 2.0f) return q;
    if (t < 2.0f / 3.0f) return p + (q - p) * (2.0f / 3.0f - t) * 6.0f;
    return p;
}

static void hsl2rgb(float h, uint8_t s_in, uint8_t l_in, uint8_t rgb[3])
{
    if (s_in == 0) {
        rgb[0] = rgb[1] = rgb[2] = 0;
    } else {
        float s = (float)s_in / 255.0f, l = (float)l_in / 255.0f;
        float q = l < 0.5f ? l * (1.0f + s) : l + s - l * s;
        float p = 2.0f * l - q;
        rgb[0] = hue2rgb(p, q, h + 1.0f / 3.0f) * 255.0f;
        rgb[1] = hue2rgb(p, q, h) * 255;
        rgb[2] = hue2rgb(p, q, h - 1.0f / 3.0f) * 255.0f;
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
            hsl2rgb((float)h / 63.0f, 255, l * 255 / 7, rgb);
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
