#include <vellum/shell.h>

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

#include <vellum/arch/io.h>

static int in_handler(struct shell_instance *inst, int argc, char **argv)
{
    if (argc < 3) {
        fprintf(stderr, "usage: %s addr size\n", argv[0]);
        return 1;
    }

    uint16_t addr = strtol(argv[1], NULL, 16);

    switch (*argv[2]) {
    case 'b':
    case 'B':
        printf("%02X\n", VlA_In8(addr));
        break;
    case 'w':
    case 'W':
        printf("%04X\n", VlA_In16(addr));
        break;
    case 'd':
    case 'D':
        printf("%08" PRIX32 "\n", VlA_In32(addr));
        break;
    default:
        return 1;
    }

    return 0;
}

static struct command in_command = {
    .name = "in",
    .handler = in_handler,
    .help_message = "Read input from I/O port",
};

static void in_command_init(void)
{
    VlShell_RegisterCommand(&in_command);
}

REGISTER_SHELL_COMMAND(in, in_command_init)
