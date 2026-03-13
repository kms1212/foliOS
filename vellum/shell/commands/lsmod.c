#include <vellum/shell.h>

#include <stdio.h>

#include <vellum/module.h>

static int lsmod_handler(struct shell_instance *inst, int argc, char **argv)
{
    struct module *current = VlModule_GetFirst();

    while (current) {
        printf("%s: base=%08zX\n", current->name, current->load_vpn * PAGE_SIZE);

        current = current->next;
    }

    return 0;
}

static struct command lsmod_command = {
    .name = "lsmod",
    .handler = lsmod_handler,
    .help_message = "List loaded modules",
};

static void lsmod_command_init(void)
{
    VlShell_RegisterCommand(&lsmod_command);
}

REGISTER_SHELL_COMMAND(lsmod, lsmod_command_init)
