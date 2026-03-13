#include <vellum/shell.h>

#include <stdio.h>
#include <stdlib.h>

#include <getopt.h>

#include <vellum/plat/bios/video.h>

#include <vellum/device.h>
#include <vellum/interface/console.h>
#include <vellum/interface/framebuffer.h>
#include <vellum/interface/video.h>
#include <vellum/status.h>

static const struct option opts[] = {
    {"list", optional_argument, 0, 'l'},
    {NULL, 0, NULL, 0},
};

static int list_modes(char *argv0)
{
    status_t status;
    struct device *fbdev;
    const struct video_interface *vidif;
    int mode;
    struct video_mode_info mode_info;

    status = VlDev_Find("video0", &fbdev);
    if (!CHECK_SUCCESS(status)) return 1;

    status = fbdev->driver->get_interface(fbdev, "video", (const void **)&vidif);
    if (!CHECK_SUCCESS(status)) return 1;

    mode = -1;
    do {
        status = vidif->get_mode_info(fbdev, mode, &mode_info);
        if (!CHECK_SUCCESS(status)) return 1;

        mode = mode_info.next_mode;

        if (mode_info.text) {
            printf("t%dx%d\t", mode_info.width, mode_info.height);
        } else {
            printf("%dx%dx%d\t", mode_info.width, mode_info.height, mode_info.bpp);
        }
    } while (mode_info.next_mode >= 0);

    printf("\n");

    return 0;
}

static int set_mode(char *argv0, int text, int width, int height, int bpp)
{
    status_t status;
    struct device *fbdev;
    const struct video_interface *vidif;
    int mode, new_mode;
    struct video_mode_info mode_info;

    status = VlDev_Find("video0", &fbdev);
    if (!CHECK_SUCCESS(status)) return 1;

    status = fbdev->driver->get_interface(fbdev, "video", (const void **)&vidif);
    if (!CHECK_SUCCESS(status)) return 1;

    mode = -1;
    new_mode = -1;
    do {
        status = vidif->get_mode_info(fbdev, mode, &mode_info);
        if (!CHECK_SUCCESS(status)) return 1;

        mode = mode_info.next_mode;

        if (text) {
            if (!mode_info.text) continue;
        } else {
            if (mode_info.text) continue;
            if (mode_info.bpp != bpp) continue;
        }

        if (mode_info.width != width) continue;
        if (mode_info.height != height) continue;

        new_mode = mode_info.current_mode;

        break;
    } while (mode_info.next_mode >= 0);

    if (new_mode < 0) {
        fprintf(stderr, "%s: video mode not found\n", argv0);
        return 1;
    }

    status = vidif->set_mode(fbdev, new_mode);
    if (!CHECK_SUCCESS(status)) return 1;

    return 0;
}

static int vmode_handler(struct shell_instance *inst, int argc, char **argv)
{
    int opt, list = 0;
    char *heightstr, *bppstr;
    int width, height, bpp;

    if (argc < 2) {
        printf("usage: %s [-l] [mode]\n", argv[0]);
        return 1;
    }

    getopt_init();
    while ((opt = getopt_long(argc, argv, "l", opts, NULL)) != -1) {
        switch (opt) {
        case 'l':
            list = 1;
            break;
        default:
            printf("usage: %s [-l] [mode]\n", argv[0]);
            return 1;
        }
    }

    if (list) {
        return list_modes(argv[0]);
    }

    if (argv[1][0] == 't') {
        width = strtol(argv[1] + 1, &heightstr, 10);
        if (!heightstr || !heightstr[0]) {
            fprintf(stderr, "%s: invalid video mode\n", argv[0]);
            return 1;
        }
        height = strtol(heightstr + 1, NULL, 10);

        return set_mode(argv[0], 1, width, height, 0);
    } else {
        width = strtol(argv[1], &heightstr, 10);
        if (!heightstr || !heightstr[0]) {
            fprintf(stderr, "%s: invalid video mode\n", argv[0]);
            return 1;
        }
        height = strtol(heightstr + 1, &bppstr, 10);
        if (!bppstr || !bppstr[0]) {
            fprintf(stderr, "%s: invalid video mode\n", argv[0]);
            return 1;
        }
        bpp = strtol(bppstr + 1, NULL, 10);

        return set_mode(argv[0], 0, width, height, bpp);
    }

    return 0;
}

static struct command vmode_command = {
    .name = "vmode",
    .handler = vmode_handler,
    .help_message = "List or set video mode",
};

static void vmode_command_init(void)
{
    VlShell_RegisterCommand(&vmode_command);
}

REGISTER_SHELL_COMMAND(vmode, vmode_command_init)
