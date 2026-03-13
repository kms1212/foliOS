#include <vellum/shell.h>

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

#include <vellum/plat/bios/disk.h>

static int drvinfo_handler(struct shell_instance *inst, int argc, char **argv)
{
    status_t status;

    if (argc < 2) {
        fprintf(stderr, "usage: %s drvnum\n", argv[0]);
        return 1;
    }

    uint8_t drive = strtol(argv[1], NULL, 16);
    uint8_t drive_type;
    struct chs drive_geometry;

    status = VlBiosP_GetDiskParams(drive, NULL, &drive_type, &drive_geometry, NULL);
    if (!CHECK_SUCCESS(status)) return 1;

    static const char *const drive_types[] = {
        "Hard disk",
        "360k",
        "1.2M",
        "720k",
        "1.44M",
        "2.88M",
        "2.88M",
    };

    printf("Type: %s\n", drive_types[drive_type]);
    printf(
        "Geometry: cylinder=%d, head=%d, sector=%d\n",
        drive_geometry.cylinder,
        drive_geometry.head,
        drive_geometry.sector
    );

    struct bios_extended_drive_params params;
    uint8_t edd_version;
    status = VlBiosP_CheckDiskExtension(drive, &edd_version, NULL);
    if (!CHECK_SUCCESS(status)) {
        printf("EDD Not supported\n");
        return 0;
    }

    switch (edd_version) {
    case EDD_VER_1_X:
        printf("EDD Version: 1.x\n");
        break;
    case EDD_VER_2_0:
        printf("EDD Version: 2.0\n");
        break;
    case EDD_VER_2_1:
        printf("EDD Version: 2.1\n");
        break;
    case EDD_VER_3_0:
        printf("EDD Version: 3.0\n");
        break;
    default:
        printf("Unknown EDD Version: %02X\n", edd_version);
        break;
    }

    status = VlBiosP_GetDiskParamsExtended(drive, &params);
    if (!CHECK_SUCCESS(status)) return 1;

    if (edd_version >= EDD_VER_1_X) {
        printf("Flags: %04X\n", params.flags);
        printf(
            "Geometry: cylinder=%" PRIu32 ", head=%" PRIu32 ", sector=%" PRIu32 "\n",
            params.geom_cylinders,
            params.geom_heads,
            params.geom_sectors
        );
        printf("Bytes per sector: %" PRIu16 "\n", params.bytes_per_sector);
        printf("Total sectors: %" PRIu64 "\n", params.total_sectors);
    }

    if (edd_version >= EDD_VER_3_0 && params.device_path_signature == 0xBEDD) {
        printf("Host bus: %4s\n", params.host_bus);
        printf("Interface: %8s\n", params.interface);
    }

    return 0;
}

static struct command drvinfo_command = {
    .name = "drvinfo",
    .handler = drvinfo_handler,
    .help_message = "Show drive information",
};

static void drvinfo_command_init(void)
{
    VlShell_RegisterCommand(&drvinfo_command);
}

REGISTER_SHELL_COMMAND(drvinfo, drvinfo_command_init)
