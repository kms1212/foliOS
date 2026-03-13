#include <vellum/shell.h>

#include <stdio.h>
#include <stdlib.h>

#include <vellum/arch/io.h>

static int out_handler(struct shell_instance *inst, int argc, char **argv)
{
    if (argc < 4) {
        fprintf(stderr, "usage: %s addr size value\n", argv[0]);
        return 1;
    }

    uint16_t addr = strtol(argv[1], NULL, 16);
    uint32_t value = strtoul(argv[3], NULL, 16);

    switch (*argv[2]) {
    case 'b':
    case 'B':
        VlA_Out8(addr, value);
        break;
    case 'w':
    case 'W':
        VlA_Out16(addr, value);
        break;
    case 'd':
    case 'D':
        VlA_Out32(addr, value);
        break;
    default:
        return 1;
    }

    return 0;
}

static struct command out_command = {
    .name = "out",
    .handler = out_handler,
    .help_message = "Write output to I/O port",
};

static void out_command_init(void)
{
    VlShell_RegisterCommand(&out_command);
}

REGISTER_SHELL_COMMAND(out, out_command_init)
