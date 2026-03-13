#include <vellum/shell.h>

#include <stdio.h>
#include <string.h>

#include <vellum/path.h>

static int readfile_handler(struct shell_instance *inst, int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: %s path\n", argv[0]);
        return 1;
    }

    char path[PATH_MAX];
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

    FILE *fp = fopen(path, "r");
    if (!fp) {
        fprintf(stderr, "%s: failed to open file\n", argv[0]);
        return 1;
    }

    char buf[512];
    int read_count;
    while ((read_count = fread(buf, 1, sizeof(buf), fp)) > 0) {
        printf("%.*s", read_count, buf);
    }
    printf("\n");

    fclose(fp);

    return 0;
}

static struct command readfile_command = {
    .name = "readfile",
    .handler = readfile_handler,
    .help_message = "Read entire file to stdio",
};

static void readfile_command_init(void)
{
    VlShell_RegisterCommand(&readfile_command);
}

REGISTER_SHELL_COMMAND(readfile, readfile_command_init)
