#include <vellum/shell.h>

#include <stdio.h>

#include <vellum/filesystem.h>

static int lsfs_handler(struct shell_instance *inst, int argc, char **argv)
{
    struct filesystem *current = VlFs_GetFirst();

    while (current) {
        printf("%s: type=%s dev=%s\n", current->name, current->driver->name, current->dev->name);

        current = current->next;
    }

    return 0;
}

static struct command lsfs_command = {
    .name = "lsfs",
    .handler = lsfs_handler,
    .help_message = "List filesystems",
};

static void lsfs_command_init(void)
{
    VlShell_RegisterCommand(&lsfs_command);
}

REGISTER_SHELL_COMMAND(lsfs, lsfs_command_init)
