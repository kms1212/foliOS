#include <vellum/shell.h>

#include <stdio.h>
#include <string.h>

#include <getopt.h>

#include <vellum/device.h>
#include <vellum/interface/char.h>

static const struct option opts[] = {
    {"device", optional_argument, 0, 'd'},
    {NULL, 0, NULL, 0},
};

static int echo_handler(struct shell_instance *inst, int argc, char **argv)
{
    status_t status;

    if (argc < 2) return 0;

    int opt;
    const char *device_name = NULL;
    getopt_init();
    while ((opt = getopt_long(argc, argv, "d:", opts, NULL)) != -1) {
        switch (opt) {
        case 'd':
            device_name = optarg;
            break;
        default:
            printf("usage: %s [-d device] [message ...]\n", argv[0]);
            return 1;
        }
    }

    if (device_name) {
        struct device *dev;
        status = VlDev_Find(device_name, &dev);
        if (!CHECK_SUCCESS(status)) {
            fprintf(stderr, "%s: cannot find device: 0x%08lX\n", argv[0], status);
            return 1;
        }

        const struct char_interface *cif;
        status = dev->driver->get_interface(dev, "char", (const void **)&cif);
        if (!CHECK_SUCCESS(status)) {
            fprintf(stderr, "%s: device is not a char device: 0x%08lX\n", argv[0], status);
            return 1;
        }

        for (int i = optind; i < argc; i++) {
            cif->write(dev, argv[i], strlen(argv[i]), NULL);
            if (i != argc - 1) {
                cif->write(dev, " ", 1, NULL);
            }
        }
        cif->write(dev, "\n", 1, NULL);
    } else {
        for (int i = optind; i < argc; i++) {
            fputs(argv[i], stdout);
            if (i != argc - 1) {
                putc(' ', stdout);
            }
        }
        putc('\n', stdout);
    }

    return 0;
}

static struct command echo_command = {
    .name = "echo",
    .handler = echo_handler,
    .help_message = "Write arguments to the stanard output",
};

static void echo_command_init(void)
{
    VlShell_RegisterCommand(&echo_command);
}

REGISTER_SHELL_COMMAND(echo, echo_command_init)
