#include <endian.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vellum/plat/panic.h>

#include <vellum/device.h>
#include <vellum/disk.h>
#include <vellum/filesystem.h>
#include <vellum/interface/block.h>
#include <vellum/status.h>
#include <vellum/types.h>

#include "folifs.h"

struct folifs_data {
    struct device *blkdev;
    const struct block_interface *blkif;

    uint16_t reserved_sectors;
    uint8_t sectors_per_block;
    uint16_t sector_size;
    uint32_t block_size;
    uint64_t udb_pointer;
    uint64_t jbb_pointer;
    uint64_t rbb_pointer;
    uint64_t group0_gbb_pointer;
    uint64_t root_mdb_pointer;

    int64_t blkbuf_num;
    uint8_t *blkbuf;
};

struct folifs_dir_data {
    uint64_t block;
    uint16_t offset;
};

static lba_t block_to_sector(struct filesystem *fs, uint64_t block)
{
    struct folifs_data *data = (struct folifs_data *)fs->data;

    return (lba_t)((uint64_t)data->reserved_sectors + (block * data->sectors_per_block));
}

static VlStatus read_block(struct filesystem *fs, uint64_t block)
{
    struct folifs_data *data = (struct folifs_data *)fs->data;
    VlStatus status;
    lba_t lba;

    lba = block_to_sector(fs, block);
    if (data->blkbuf_num == lba) return STATUS_SUCCESS;

    status = data->blkif->read(data->blkdev, lba, data->blkbuf, data->sectors_per_block, NULL);
    if (!CHECK_SUCCESS(status)) return status;

    data->blkbuf_num = (int64_t)block;

    return STATUS_SUCCESS;
}

static VlStatus probe(struct device *dev, struct fs_driver *drv);
static VlStatus mount(
    struct filesystem **fsout, struct fs_driver *drv, struct device *dev, const char *name
);
static VlStatus unmount(struct filesystem *fs);

static VlStatus open(struct fs_directory *dir, const char *name, struct fs_file **fileout);
static VlStatus read(struct fs_file *file, void *buf, size_t len, size_t *result);
static VlStatus seek(struct fs_file *file, off_t offset, int origin);
static VlStatus tell(struct fs_file *file, off_t *result);
static void close(struct fs_file *file);

static VlStatus open_root_directory(struct filesystem *fs, struct fs_directory **dirout);
static VlStatus open_directory(
    struct fs_directory *dir, const char *name, struct fs_directory **dirout
);
static VlStatus rewind_directory(struct fs_directory *dir);
static VlStatus iter_directory(struct fs_directory *dir, struct fs_directory_entry *entry);
static void close_directory(struct fs_directory *dir);

static void folifs_init(void)
{
    VlStatus status;
    struct fs_driver *drv;

    status = VlFs_CreateDriver(&drv);
    if (!CHECK_SUCCESS(status)) {
        VlP_Panic(status, "cannot register fs driver \"folifs\"");
    }

    drv->name = "folifs";
    drv->probe = probe;
    drv->mount = mount;
    drv->unmount = unmount;
    drv->open = open;
    drv->read = read;
    drv->seek = seek;
    drv->tell = tell;
    drv->close = close;
    drv->open_root_directory = open_root_directory;
    drv->open_directory = open_directory;
    drv->rewind_directory = rewind_directory;
    drv->iter_directory = iter_directory;
    drv->close_directory = close_directory;
}

static VlStatus probe(struct device *dev, struct fs_driver *drv)
{
    VlStatus status;
    struct device *blkdev = NULL;
    const struct block_interface *blkif = NULL;
    size_t block_size;
    struct folifs_first_sector lba0;

    blkdev = dev;
    if (!blkdev) return STATUS_INVALID_VALUE;

    status = blkdev->driver->get_interface(blkdev, "block", (const void **)&blkif);
    if (!CHECK_SUCCESS(status)) return status;

    status = blkif->get_block_size(blkdev, &block_size);
    if (!CHECK_SUCCESS(status)) return status;
    if (block_size != 512) return STATUS_SIZE_CHECK_FAILURE;

    /* read sector 0 */
    status = blkif->read(blkdev, 0, &lba0, 1, NULL);
    if (!CHECK_SUCCESS(status)) return status;

    /* check signatures */
    if (le16toh(lba0.vbr_signature) != FOLIFS_VBR_SIGNATURE) {
        return STATUS_INVALID_SIGNATURE;
    }

    if (strncmp(
            lba0.filesystem_signature,
            FOLIFS_FS_SIGNATURE,
            sizeof(lba0.filesystem_signature)
        ) != 0) {
        return STATUS_INVALID_SIGNATURE;
    }

    return STATUS_SUCCESS;
}

static VlStatus mount(
    struct filesystem **fsout, struct fs_driver *drv, struct device *dev, const char *name
)
{
    VlStatus status;
    struct filesystem *fs = NULL;
    struct device *blkdev = NULL;
    const struct block_interface *blkif = NULL;
    size_t block_size;
    struct folifs_data *data = NULL;
    struct folifs_first_sector lba0;
    struct folifs_rdb rdb;
    const struct folifs_rdb *rdbp = NULL;

    blkdev = dev;
    if (!blkdev) {
        status = STATUS_INVALID_VALUE;
        goto has_error;
    }

    status = blkdev->driver->get_interface(blkdev, "block", (const void **)&blkif);
    if (!CHECK_SUCCESS(status)) goto has_error;

    status = blkif->get_block_size(blkdev, &block_size);
    if (!CHECK_SUCCESS(status)) return status;
    if (block_size != 512) return STATUS_SIZE_CHECK_FAILURE;

    status = VlFs_Create(&fs, drv, dev, name);
    if (!CHECK_SUCCESS(status)) goto has_error;

    data = malloc(sizeof(*data));
    if (!data) {
        status = STATUS_UNKNOWN_ERROR;
        goto has_error;
    }

    data->blkdev = blkdev;
    data->blkif = blkif;
    data->blkbuf = NULL;
    data->blkbuf_num = -1;
    fs->data = data;

    /* read sector 0 */
    status = blkif->read(blkdev, 0, &lba0, 1, NULL);
    if (!CHECK_SUCCESS(status)) goto has_error;

    /* check signatures */
    if (le16toh(lba0.vbr_signature) != FOLIFS_VBR_SIGNATURE) {
        status = STATUS_INVALID_SIGNATURE;
        goto has_error;
    }

    if ((strncmp(
             lba0.filesystem_signature,
             FOLIFS_FS_SIGNATURE,
             sizeof(lba0.filesystem_signature)
         ) != 0)) {
        status = STATUS_INVALID_SIGNATURE;
        goto has_error;
    }

    data->reserved_sectors = lba0.reserved_sectors;

    /* read the first sector of RDB */
    status = blkif->read(blkdev, data->reserved_sectors, &rdb, 1, NULL);
    if (!CHECK_SUCCESS(status)) goto has_error;

    data->sectors_per_block = rdb.sectors_per_block;
    data->sector_size = rdb.bytes_per_sector;
    data->block_size = data->sector_size * rdb.sectors_per_block;
    data->blkbuf = malloc(data->block_size);

    /* read entire RDB */
    status = read_block(fs, 0);
    if (!CHECK_SUCCESS(status)) goto has_error;

    rdbp = (const struct folifs_rdb *)data->blkbuf;

    data->udb_pointer = rdbp->udb_pointer;
    data->jbb_pointer = rdbp->jbb_pointer;
    data->rbb_pointer = rdbp->rbb_pointer;
    data->group0_gbb_pointer = rdbp->group0_gbb_pointer;
    data->root_mdb_pointer = rdbp->root_mdb_pointer;

    return STATUS_SUCCESS;

has_error:
    if (data && data->blkbuf) {
        free(data->blkbuf);
    }

    if (data) {
        free(data);
    }

    if (fs) {
        VlFs_Remove(fs);
    }

    return status;
}

static VlStatus unmount(struct filesystem *fs)
{
    struct folifs_data *data = (struct folifs_data *)fs->data;

    free(data->blkbuf);

    free(data);

    VlFs_Remove(fs);

    return STATUS_SUCCESS;
}

static VlStatus open(struct fs_directory *dir, const char *name, struct fs_file **fileout)
{
    return STATUS_NOT_IMPLEMENTED;
}

static VlStatus read(struct fs_file *file, void *buf, size_t len, size_t *result)
{
    return STATUS_NOT_IMPLEMENTED;
}

static VlStatus seek(struct fs_file *file, off_t offset, int origin)
{
    return STATUS_NOT_IMPLEMENTED;
}

static VlStatus tell(struct fs_file *file, off_t *result)
{
    return STATUS_NOT_IMPLEMENTED;
}

static void close(struct fs_file *file) {}

static VlStatus open_root_directory(struct filesystem *fs, struct fs_directory **dirout)
{
    // struct folifs_data *data = (struct folifs_data *)fs->data;
    //
    // struct folifs_dir_data *dir_data = malloc(sizeof(*dir_data));
    // dir_data->block = data->root_mdb_pointer;
    // dir_data->offset = offsetof(struct folifs_acb, entries);
    //
    // struct fs_directory *dir = malloc(sizeof(*dir));
    // dir->fs = fs;
    // dir->data = dir_data;

    return STATUS_NOT_IMPLEMENTED;
}

static VlStatus open_directory(
    struct fs_directory *dir, const char *name, struct fs_directory **dirout
)
{
    return STATUS_NOT_IMPLEMENTED;
}

static VlStatus rewind_directory(struct fs_directory *dir)
{
    return STATUS_NOT_IMPLEMENTED;
}

static VlStatus iter_directory(struct fs_directory *dir, struct fs_directory_entry *entry)
{
    return STATUS_NOT_IMPLEMENTED;
}

static void close_directory(struct fs_directory *dir) {}

REGISTER_FILESYSTEM_DRIVER(folifs, folifs_init)
