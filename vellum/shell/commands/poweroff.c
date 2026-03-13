#include <vellum/shell.h>

#include <vellum/plat/power.h>

__noreturn static int poweroff_handler(struct shell_instance *inst, int argc, char **argv)
{
    VlP_Poweroff();
}

static struct command poweroff_command = {
    .name = "poweroff",
    .handler = poweroff_handler,
    .help_message = "Power off the computer",
};

static void poweroff_command_init(void)
{
    VlShell_RegisterCommand(&poweroff_command);
}

REGISTER_SHELL_COMMAND(poweroff, poweroff_command_init)
