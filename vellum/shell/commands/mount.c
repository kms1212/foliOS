#include <vellum/shell.h>

#include <stdio.h>

#include <vellum/filesystem.h>

static int mount_handler(struct shell_instance *inst, int argc, char **argv)
{
    status_t status;

    if (argc < 3) {
        fprintf(stderr, "usage: %s device fs_name\n", argv[0]);
        return 1;
    }

    struct device *blkdev;
    status = VlDev_Find(argv[1], &blkdev);
    if (!CHECK_SUCCESS(status)) {
        fprintf(stderr, "%s: could not find device\n", argv[0]);
        return 1;
    }

    status = VlFs_MountAuto(blkdev, argv[2]);
    if (!CHECK_SUCCESS(status)) return 1;

    return 0;
}

static struct command mount_command = {
    .name = "mount",
    .handler = mount_handler,
    .help_message = "Mount filesystem",
};

static void mount_command_init(void)
{
    VlShell_RegisterCommand(&mount_command);
}

REGISTER_SHELL_COMMAND(mount, mount_command_init)
