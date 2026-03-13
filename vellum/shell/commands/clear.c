#include <vellum/shell.h>

#include <stdio.h>

static int clear_handler(struct shell_instance *inst, int argc, char **argv)
{
    printf("\x1b[3J\x1b[0;0f");

    return 0;
}

static struct command clear_command = {
    .name = "clear",
    .handler = clear_handler,
    .help_message = "Clear console",
};

static void clear_command_init(void)
{
    VlShell_RegisterCommand(&clear_command);
}

REGISTER_SHELL_COMMAND(clear, clear_command_init)
