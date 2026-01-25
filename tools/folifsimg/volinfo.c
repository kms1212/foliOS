#include "command.h"

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "folifs.h"

static const struct option options[] = {
    {"image", 1, NULL, 'i'},
    {0, 0, 0, 0},
};

int volinfo_handler(int argc, char **argv)
{
    int next_option;
    const char *image_path;
    int readonly = 1;

    do {
        next_option = getopt_long(argc, argv, "i:", options, NULL);

        switch (next_option) {
        case 'i':
            image_path = optarg;
            break;
        case '?':
            return 1;
        case -1:
            break;
        default:
            exit(1);
        }
    } while (next_option != -1);

    printf("%s\n", image_path);

    int ret = open_image(image_path, readonly);
    if (ret) {
        exit(1);
    }

    void *buf = malloc(512);

    uint64_t total_sector_count, total_block_count, root_mdb_pointer, udb_pointer, jbb_pointer,
        rbb_pointer, group0_gbb_pointer;
    uint32_t flags;
    uint16_t reserved_sectors, bytes_per_sector, filesystem_version;
    uint8_t sectors_per_block, rdb_copy_count;
    folifs_uuid_t volume_uuid;
    char formatted_os[16];

    /* read the first sector of image */
    long io_count = read_sector(buf, 0, 1);
    if (io_count != 1) {
        close_image();
        exit(1);
    }

    /* validate signature & get values */
    struct folifs_first_sector *sect0 = buf;
    if (sect0->filesystem_signature[0] != 'A' || sect0->filesystem_signature[1] != 'F' ||
        sect0->filesystem_signature[2] != 'S' || sect0->filesystem_signature[3] != '\0') {
        fprintf(stderr, "%s: invalid filesystem signature\n", argv0);
        close_image();
        exit(1);
    }
    if (sect0->vbr_signature != 0xAA55) {
        fprintf(stderr, "%s: invalid vbr signature\n", argv0);
        close_image();
        exit(1);
    }
    reserved_sectors = sect0->reserved_sectors;

    /* read the first sector of RDB */
    io_count = read_sector(buf, reserved_sectors, 1);
    if (io_count != 1) {
        close_image();
        exit(1);
    }

    /* get sectors per block */
    struct folifs_rdb *rdb = buf;
    sectors_per_block = rdb->sectors_per_block;

    /* reallocate buffer */
    buf = realloc(buf, 512 * sectors_per_block);

    /* read entire RDB */
    io_count = read_sector(buf, reserved_sectors, sectors_per_block);
    if (io_count != sectors_per_block) {
        close_image();
        exit(1);
    }

    /* get values */
    rdb = buf;
    total_sector_count = rdb->total_sector_count;
    total_block_count = rdb->total_block_count;
    rdb_copy_count = rdb->rdb_copy_count;
    bytes_per_sector = rdb->bytes_per_sector;
    flags = rdb->flags;
    root_mdb_pointer = rdb->root_mdb_pointer;
    udb_pointer = rdb->udb_pointer;
    jbb_pointer = rdb->jbb_pointer;
    rbb_pointer = rdb->rbb_pointer;
    group0_gbb_pointer = rdb->group0_gbb_pointer;
    memcpy(&volume_uuid, &rdb->volume_uuid, sizeof(volume_uuid));
    memcpy(formatted_os, rdb->formatted_os, sizeof(formatted_os));
    filesystem_version = rdb->filesystem_version;

    /* read root MDB */
    io_count = read_sector(
        buf,
        reserved_sectors + root_mdb_pointer * sectors_per_block,
        sectors_per_block
    );
    if (io_count != sectors_per_block) {
        close_image();
        exit(1);
    }

    /* get values */
    struct folifs_mdb *mdb = buf;
    int mdb_offset = sizeof(struct folifs_mdb);
    union {
        struct folifs_mdb_attribute_entry_header header;
        struct folifs_mdb_fae fae;
        struct folifs_mdb_tae tae;
        struct folifs_mdb_pae pae;
        struct folifs_mdb_qae qae;
        struct folifs_mdb_eae qee;
        struct folifs_mdb_cae cae;
    } *entry;
    do {
        entry = (void *)((uint8_t *)buf + mdb_offset);

        if (mdb->next_mdb_pointer && entry->header.entry_type == 0) {
            mdb_offset = sizeof(struct folifs_mdb);

            /* read next root MDB */
            io_count = read_sector(
                buf,
                reserved_sectors + mdb->next_mdb_pointer * sectors_per_block,
                sectors_per_block
            );
            if (io_count != sectors_per_block) {
                close_image();
                exit(1);
            }

            continue;
        }

        switch (entry->header.entry_type) {
        case MDB_ENTRY_FAE:
        case MDB_ENTRY_TAE:
        case MDB_ENTRY_PAE:
        case MDB_ENTRY_QAE:
        case MDB_ENTRY_EAE:
        case MDB_ENTRY_CAE:
        default:
            break;
        }
    } while (entry->header.entry_type != 0);

    printf("Total Sector Count: %llu\n", total_sector_count);
    printf("Total Block Count: %llu\n", total_block_count);
    printf("RDB Copy Count: %u\n", rdb_copy_count);
    printf("Bytes Per Sector :%u\n", bytes_per_sector);
    printf("Flags: %u\n", flags);
    printf("Root MDB Pointer: 0x%016llX\n", root_mdb_pointer);
    printf("UDB Pointer: 0x%016llX\n", udb_pointer);
    printf("JBB Pointer: 0x%016llX\n", jbb_pointer);
    printf("RBB Pointer: 0x%016llX\n", rbb_pointer);
    printf("Group 0 GBB Pointer: 0x%016llX\n", group0_gbb_pointer);
    printf(
        "Volume UUID: %02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X",
        volume_uuid.bytes[0],
        volume_uuid.bytes[1],
        volume_uuid.bytes[2],
        volume_uuid.bytes[3],
        volume_uuid.bytes[4],
        volume_uuid.bytes[5],
        volume_uuid.bytes[6],
        volume_uuid.bytes[7],
        volume_uuid.bytes[8],
        volume_uuid.bytes[9],
        volume_uuid.bytes[10],
        volume_uuid.bytes[11],
        volume_uuid.bytes[12],
        volume_uuid.bytes[13],
        volume_uuid.bytes[14],
        volume_uuid.bytes[15]
    );
    printf("Formatted OS: %s\n", formatted_os);
    printf("Filesystem Version: %u\n", filesystem_version);

    close_image();

    return 0;
}
