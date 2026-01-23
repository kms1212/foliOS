#include <vellum/shell.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>

#include <vellum/status.h>
#include <vellum/panic.h>
#include <vellum/elf.h>
#include <vellum/path.h>
#include <vellum/mm.h>
#include <vellum/module.h>

static int loadmodule_handler(struct shell_instance *inst, int argc, char **argv)
{
    status_t status;

    if (argc < 2) {
        fprintf(stderr, "usage: %s path\n", argv[0]);
        return 1;
    }

    char path[PATH_MAX];
    if (path_is_absolute(argv[1])) {
        strncpy(path, argv[1], sizeof(path) - 1);
    } else {
        strncpy(path, inst->working_dir_path, sizeof(path) - 1);
        path_join(path, sizeof(path), argv[1]);

        if (!inst->fs) {
            fprintf(stderr, "%s: filesystem not selected\n", argv[0]);
            return 1;
        }
    }

    status = module_load(path, NULL);
    if (!CHECK_SUCCESS(status)) {
        fprintf(stderr, "%s: failed to load module: %08X\n", argv[0], status);
        return 1;
    }

    return 0;
}

static struct command loadmodule_command = {
    .name = "loadmodule",
    .handler = loadmodule_handler,
    .help_message = "Load bootloader module",
};

static void loadmodule_command_init(void)
{
    shell_command_register(&loadmodule_command);
}

REGISTER_SHELL_COMMAND(loadmodule, loadmodule_command_init)
