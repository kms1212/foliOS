#include <vellum/shell.h>

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include <vellum/device.h>
#include <vellum/interface/framebuffer.h>
#include <vellum/interface/video.h>
#include <vellum/path.h>
#include <vellum/status.h>

static int dispimg_handler(struct shell_instance *inst, int argc, char **argv)
{
    status_t status;

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

    struct device *fbdev;
    status = VlDev_Find("video0", &fbdev);
    if (!CHECK_SUCCESS(status)) {
        fprintf(stderr, "%s: cannot find device\n", argv[0]);
        fclose(fp);
        return 1;
    }

    const struct video_interface *vidif;
    status = fbdev->driver->get_interface(fbdev, "video", (const void **)&vidif);
    if (!CHECK_SUCCESS(status)) {
        fclose(fp);
        return 1;
    }

    const struct framebuffer_interface *fbif;
    status = fbdev->driver->get_interface(fbdev, "framebuffer", (const void **)&fbif);
    if (!CHECK_SUCCESS(status)) {
        fclose(fp);
        return 1;
    }

    int current_vmode;
    struct video_mode_info vmode_info;
    uint32_t *framebuffer;
    status = fbif->get_framebuffer(fbdev, (void **)&framebuffer);
    if (!CHECK_SUCCESS(status)) {
        fclose(fp);
        return 1;
    }

    status = vidif->get_mode(fbdev, &current_vmode);
    if (!CHECK_SUCCESS(status)) {
        fclose(fp);
        return 1;
    }

    status = vidif->get_mode_info(fbdev, current_vmode, &vmode_info);
    if (!CHECK_SUCCESS(status)) {
        fclose(fp);
        return 1;
    }

    struct bmp_header {
        char signature[2];
        uint32_t file_size;
        uint8_t unused[4];
        uint32_t bitmap_offset;
    } __packed;

    struct bmp_dibheader_core {
        uint32_t header_size;
        uint32_t width;
        uint32_t height;
        uint16_t color_planes;
        uint16_t bpp;
    } __packed;

    struct bmp_header header;
    fread(&header, sizeof(header), 1, fp);

    if (header.signature[0] != 'B' || header.signature[1] != 'M') {
        fprintf(stderr, "%s: not a BMP file\n", argv[0]);
        fclose(fp);
        return 1;
    }

    struct bmp_dibheader_core dibheader;
    fread(&dibheader, sizeof(dibheader), 1, fp);

    printf(
        "bmp file size: %" PRIu32 " bytes, bitmap offset %" PRIu32 "\n",
        header.file_size,
        header.bitmap_offset
    );
    printf(
        "width: %" PRIu32 ", height: %" PRIu32 ", bpp: %" PRIu16 "\n",
        dibheader.width,
        dibheader.height,
        dibheader.bpp
    );

    fseek(fp, header.bitmap_offset, SEEK_SET);

    int ystart = (vmode_info.height - dibheader.height) / 2;
    int xstart = (vmode_info.width - dibheader.width) / 2;

    for (uint32_t y = 0; y < dibheader.height; y++) {
        for (uint32_t x = 0; x < dibheader.width; x++) {
            uint32_t buf;
            fread(&buf, dibheader.bpp / 8, 1, fp);
            framebuffer[(ystart + dibheader.height - y - 1) * vmode_info.width + xstart + x] = buf;
        }
    }
    fbif->invalidate(fbdev, 0, 0, vmode_info.width - 1, vmode_info.height - 1);
    fbif->flush(fbdev);

    fclose(fp);

    char ch;
    fread(&ch, sizeof(ch), 1, stdin);

    fputs("\x1b[3J\x1b[0;0f", stdout);

    return 0;
}

static struct command dispimg_command = {
    .name = "dispimg",
    .handler = dispimg_handler,
    .help_message = "Show BMP file",
};

static void dispimg_command_init(void)
{
    VlShell_RegisterCommand(&dispimg_command);
}

REGISTER_SHELL_COMMAND(dispimg, dispimg_command_init)
