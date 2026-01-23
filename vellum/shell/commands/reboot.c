#include <vellum/shell.h>

#include <vellum/asm/power.h>

__noreturn
static int reboot_handler(struct shell_instance *inst, int argc, char **argv)
{
    _pc_reboot();
}

static struct command reboot_command = {
    .name = "reboot",
    .handler = reboot_handler,
    .help_message = "Reboot the computer",
};

static void reboot_command_init(void)
{
    shell_command_register(&reboot_command);
}

REGISTER_SHELL_COMMAND(reboot, reboot_command_init)
