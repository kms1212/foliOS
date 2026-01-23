#include <vellum/shell.h>

#include <stdio.h>
#include <stdlib.h>

#include <vellum/asm/io.h>

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
            printf("%02X\n", io_in8(addr));
            break;
        case 'w':
        case 'W':
            printf("%04X\n", io_in16(addr));
            break;
        case 'd':
        case 'D':
            printf("%08lX\n", io_in32(addr));
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
    shell_command_register(&in_command);
}

REGISTER_SHELL_COMMAND(in, in_command_init)
