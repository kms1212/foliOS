#include <vellum/shell.h>

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

static int read_handler(struct shell_instance *inst, int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: %s addr [count]\n", argv[0]);
        return 1;
    }

    uintptr_t addr = strtoull(argv[1], NULL, 16);
    uint32_t count = argc < 3 ? 1 : strtoul(argv[2], NULL, 10);

    while (count > 0) {
        printf("%08" PRIX32 ": ", addr);
        for (int i = 0; i < 16 && count > 0; i++) {
            printf("%02X ", *(uint8_t *)addr);

            addr++;
            count--;
        }
        printf("\n");
    }
    return 0;
}

static struct command read_command = {
    .name = "read",
    .handler = read_handler,
    .help_message = "Read value from memory",
};

static void read_command_init(void)
{
    VlShell_RegisterCommand(&read_command);
}

REGISTER_SHELL_COMMAND(read, read_command_init)
