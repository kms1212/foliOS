#include <vellum/shell.h>

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include <zlib.h>

#include <vellum/path.h>

static int crc32_handler(struct shell_instance *inst, int argc, char **argv)
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

    uint8_t buf[512];
    int read_count;
    uint32_t checksum = 0xFFFFFFFF;
    while ((read_count = fread(buf, 1, sizeof(buf), fp)) > 0) {
        checksum = crc32(checksum, buf, read_count);
    }
    printf("%08" PRIX32 "\n", checksum);

    fclose(fp);

    return 0;
}

static struct command crc32_command = {
    .name = "crc32",
    .handler = crc32_handler,
    .help_message = "Get CRC32 checksum of a file",
};

static void crc32_command_init(void)
{
    VlShell_RegisterCommand(&crc32_command);
}

REGISTER_SHELL_COMMAND(crc32, crc32_command_init)
