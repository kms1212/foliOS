#include <vellum/shell.h>

#include <vellum/plat/power.h>

__noreturn static int reboot_handler(struct shell_instance *inst, int argc, char **argv)
{
    VlP_Reboot();
}

static struct command reboot_command = {
    .name = "reboot",
    .handler = reboot_handler,
    .help_message = "Reboot the computer",
};

static void reboot_command_init(void)
{
    VlShell_RegisterCommand(&reboot_command);
}

REGISTER_SHELL_COMMAND(reboot, reboot_command_init)
