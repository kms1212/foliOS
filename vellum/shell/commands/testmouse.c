#include <vellum/shell.h>

#include <stdio.h>

#include <vellum/device.h>
#include <vellum/hid.h>
#include <vellum/interface/framebuffer.h>
#include <vellum/interface/hid.h>
#include <vellum/interface/video.h>
#include <vellum/status.h>

static int testmouse_handler(struct shell_instance *inst, int argc, char **argv)
{
    status_t status;
    struct device *msdev;
    status = VlDev_Find("mouse0", &msdev);
    if (!CHECK_SUCCESS(status)) {
        fprintf(stderr, "%s: cannot find device\n", argv[0]);
        return 1;
    }

    const struct hid_interface *hidif;
    status = msdev->driver->get_interface(msdev, "hid", (const void **)&hidif);
    if (!CHECK_SUCCESS(status)) return 1;

    struct device *fbdev;
    status = VlDev_Find("video0", &fbdev);
    if (!CHECK_SUCCESS(status)) {
        fprintf(stderr, "%s: cannot find device\n", argv[0]);
        return 1;
    }

    const struct video_interface *vidif;
    status = fbdev->driver->get_interface(fbdev, "video", (const void **)&vidif);
    if (!CHECK_SUCCESS(status)) return 1;

    const struct framebuffer_interface *fbif;
    status = fbdev->driver->get_interface(fbdev, "framebuffer", (const void **)&fbif);
    if (!CHECK_SUCCESS(status)) return 1;

    int current_vmode;
    struct video_mode_info vmode_info;
    uint32_t *framebuffer;
    status = fbif->get_framebuffer(fbdev, (void **)&framebuffer);
    if (!CHECK_SUCCESS(status)) return 1;

    status = vidif->get_mode(fbdev, &current_vmode);
    if (!CHECK_SUCCESS(status)) return 1;

    status = vidif->get_mode_info(fbdev, current_vmode, &vmode_info);
    if (!CHECK_SUCCESS(status)) return 1;

    int xpos = vmode_info.width / 2, ypos = vmode_info.height / 2, should_exit = 0;
    uint16_t key, flags;

    puts("\x1b[3J\x1b[0;0f\x1b[?25lclose");

    while (!should_exit) {
        status = hidif->wait_event(msdev);
        status = hidif->poll_event(msdev, &key, &flags);
        if (!CHECK_SUCCESS(status)) continue;

        switch (flags & KEY_FLAG_TYPEMASK) {
        case 0:
            if (key == KEY_MOUSEBTNL && !(flags & KEY_FLAG_BREAK)) {
                if (xpos < 40 && ypos < 16) {
                    should_exit = 1;
                }
            }
            break;
        case KEY_FLAG_XMOVE:
            if ((flags & KEY_FLAG_NEGATIVE)) {
                if (xpos < key) {
                    xpos = 0;
                } else {
                    xpos -= key;
                }
            } else if (!(flags & KEY_FLAG_NEGATIVE)) {
                if (vmode_info.width <= xpos + key) {
                    xpos = vmode_info.width - 1;
                } else {
                    xpos += key;
                }
            }
            break;
        case KEY_FLAG_YMOVE:
            if ((flags & KEY_FLAG_NEGATIVE)) {
                if (vmode_info.height <= ypos + key) {
                    ypos = vmode_info.height - 1;
                } else {
                    ypos += key;
                }
            } else if (!(flags & KEY_FLAG_NEGATIVE)) {
                if (ypos < key) {
                    ypos = 0;
                } else {
                    ypos -= key;
                }
            }

            framebuffer[ypos * vmode_info.width + xpos] = 0xFFFFFF;
            fbif->invalidate(fbdev, xpos, ypos, xpos, ypos);
            fbif->flush(fbdev);
            break;
        default:
            break;
        }
    }

    puts("\x1b[3J\x1b[?25h\x1b[0;0f");

    return 0;
}

static struct command testmouse_command = {
    .name = "testmouse",
    .handler = testmouse_handler,
    .help_message = "Test mouse functions",
};

static void testmouse_command_init(void)
{
    VlShell_RegisterCommand(&testmouse_command);
}

REGISTER_SHELL_COMMAND(testmouse, testmouse_command_init)
