#include <vellum/shell.h>

#include <limits.h>
#include <stdio.h>
#include <string.h>

#include <vellum/plat/bios/misc.h>

#include <vellum/status.h>

static int bootnext_handler(struct shell_instance *inst, int argc, char **argv)
{
    VlBiosP_BootFromNextDevice();
    return 1;
}

static struct command bootnext_command = {
    .name = "bootnext",
    .handler = bootnext_handler,
    .help_message = "Boot from next device",
};

static void bootnext_command_init(void)
{
    VlShell_RegisterCommand(&bootnext_command);
}

REGISTER_SHELL_COMMAND(bootnext, bootnext_command_init)
