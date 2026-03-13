#include <vellum/shell.h>

#include <stdio.h>
#include <stdlib.h>

static int jump_handler(struct shell_instance *inst, int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: %s addr\n", argv[0]);
        return 1;
    }

    uintptr_t addr = strtoull(argv[1], NULL, 16);
    ((void (*)(void))addr)();

    return 0;
}

static struct command jump_command = {
    .name = "jump",
    .handler = jump_handler,
    .help_message = "Jump to address",
};

static void jump_command_init(void)
{
    VlShell_RegisterCommand(&jump_command);
}

REGISTER_SHELL_COMMAND(jump, jump_command_init)
