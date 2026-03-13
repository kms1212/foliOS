#include <vellum/shell.h>

#include <stdio.h>
#include <stdlib.h>

#include <vellum/debug.h>
#include <vellum/device.h>
#include <vellum/interface/block.h>
#include <vellum/status.h>

static int readblk_handler(struct shell_instance *inst, int argc, char **argv)
{
    status_t status;
    lba_t lba;
    struct device *blkdev;
    const struct block_interface *blkif;
    size_t blksize;
    uint8_t buf[2048];

    if (argc < 3) {
        fprintf(stderr, "usage: %s device lba\n", argv[0]);
        return 1;
    }

    lba = strtoull(argv[2], NULL, 16);

    status = VlDev_Find(argv[1], &blkdev);
    if (!CHECK_SUCCESS(status)) {
        fprintf(stderr, "%s: could not find device\n", argv[0]);
        return 1;
    }
    status = blkdev->driver->get_interface(blkdev, "block", (const void **)&blkif);
    if (!CHECK_SUCCESS(status)) return 1;

    status = blkif->get_block_size(blkdev, &blksize);
    if (!CHECK_SUCCESS(status)) return 1;

    if (blksize > sizeof(buf)) {
        fprintf(stderr, "%s: block size too big\n", argv[0]);
        return 1;
    }

    status = blkif->read(blkdev, lba, buf, 1, NULL);
    if (!CHECK_SUCCESS(status)) return 1;

    VlDbg_Hexdump(stdout, buf, blksize, 0);

    return 0;
}

static struct command readblk_command = {
    .name = "readblk",
    .handler = readblk_handler,
    .help_message = "Read block from block device",
};

static void readblk_command_init(void)
{
    VlShell_RegisterCommand(&readblk_command);
}

REGISTER_SHELL_COMMAND(readblk, readblk_command_init)
