#include <vellum/shell.h>

#include <stdio.h>
#include <stdlib.h>

#include <vellum/plat/bios/video.h>

static int dispinfo_handler(struct shell_instance *inst, int argc, char **argv)
{
    struct edid buf;
    int unit, block;
    status_t status;

    if (argc < 3) {
        printf("usage: %s unit block\n", argv[0]);
        return 1;
    }

    unit = strtol(argv[1], NULL, 10);
    block = strtol(argv[2], NULL, 10);

    status = VlBiosP_GetVbeDdcEdid(unit, block, &buf);
    if (!CHECK_SUCCESS(status)) {
        printf("%s: edid error\n", argv[0]);
        return 1;
    }

    uint16_t manufacturer_id =
        ((buf.manufacturer_id & 0x00FF) << 8) | ((buf.manufacturer_id & 0xFF00) >> 8);

    printf(
        "Manufacturer: %c%c%c\n",
        'A' + ((manufacturer_id & 0x7C00) >> 10),
        'A' + ((manufacturer_id & 0x03E0) >> 5),
        'A' + (manufacturer_id & 0x001F)
    );

    printf("EDID Version: %02X\n", buf.edid_version);
    printf("EDID Revision: %02X\n", buf.edid_revision);

    for (int i = 0; i < 4; i++) {
        int w = buf.detailed_timings[0].hactive |
            ((int)(buf.detailed_timings[0].hactive_hblank & 0xF0) << 4);
        int h = buf.detailed_timings[0].vactive |
            ((int)(buf.detailed_timings[0].vactive_vblank & 0xF0) << 4);

        printf("Preferred resolution #%d: %dx%d\n", i, w, h);
    }

    return 0;
}

static struct command dispinfo_command = {
    .name = "dispinfo",
    .handler = dispinfo_handler,
    .help_message = "Get information of the display",
};

static void dispinfo_command_init(void)
{
    VlShell_RegisterCommand(&dispinfo_command);
}

REGISTER_SHELL_COMMAND(dispinfo, dispinfo_command_init)
