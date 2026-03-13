#include <vellum/shell.h>

#include <stdio.h>
#include <stdlib.h>

#include <vellum/arch/io.h>

static int write_handler(struct shell_instance *inst, int argc, char **argv)
{
    if (argc < 3) {
        fprintf(stderr, "usage: %s addr value\n", argv[0]);
        return 1;
    }

    uintptr_t addr = strtoull(argv[1], NULL, 16);
    uint8_t value = strtol(argv[2], NULL, 16);

    *(uint8_t *)addr = value;

    return 0;
}

static struct command write_command = {
    .name = "write",
    .handler = write_handler,
    .help_message = "Write value to memory",
};

static void write_command_init(void)
{
    VlShell_RegisterCommand(&write_command);
}

REGISTER_SHELL_COMMAND(write, write_command_init)
