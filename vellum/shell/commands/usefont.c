#include <vellum/shell.h>

#include <stdio.h>
#include <string.h>

#include <vellum/device.h>
#include <vellum/font.h>
#include <vellum/interface/console.h>
#include <vellum/path.h>
#include <vellum/status.h>

static int usefont_handler(struct shell_instance *inst, int argc, char **argv)
{
    status_t status;

    char path[PATH_MAX];
    if (argc > 1) {
        if (VlPath_IsAbsolute(argv[1])) {
            strncpy(path, argv[1], sizeof(path) - 1);
        } else {
            strncpy(path, inst->working_dir_path, sizeof(path) - 1);
            VlPath_Join(path, sizeof(path), argv[1]);

            if (!inst->fs) {
                fprintf(stderr, "%s: filesystem not selected\n", argv[0]);
                return 1;
            }
        }
    }

    int ret = VlFont_Use(argc > 1 ? path : NULL);
    if (ret) return 1;

    struct device *condev;
    status = VlDev_Find("console0", &condev);
    if (!CHECK_SUCCESS(status)) return 1;

    const struct console_interface *conif;
    status = condev->driver->get_interface(condev, "console", (const void **)&conif);
    if (!CHECK_SUCCESS(status)) return 1;

    int width, height;
    conif->get_dimension(condev, &width, &height);

    conif->invalidate(condev, 0, 0, width - 1, height - 1);
    conif->flush(condev);

    return 0;
}

static struct command usefont_command = {
    .name = "usefont",
    .handler = usefont_handler,
    .help_message = "Change terminal font",
};

static void usefont_command_init(void)
{
    VlShell_RegisterCommand(&usefont_command);
}

REGISTER_SHELL_COMMAND(usefont, usefont_command_init)
