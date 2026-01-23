#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <endian.h>

#include <vellum/compiler.h>
#include <vellum/status.h>
#include <vellum/device.h>
#include <vellum/filesystem.h>
#include <vellum/disk.h>
#include <vellum/interface/block.h>

#include "folifs.h"

struct folifs_data {
    struct device *blkdev;
    const struct block_interface *blkif;

    uint16_t    reserved_sectors;
    uint8_t     sectors_per_block;
    uint16_t    sector_size;
    uint32_t    block_size;
    uint64_t    udb_pointer;
    uint64_t    jbb_pointer;
    uint64_t    rbb_pointer;
    uint64_t    group0_gbb_pointer;
    uint64_t    root_mdb_pointer;

    int64_t     blkbuf_num;
    uint8_t     *blkbuf;
};

struct folifs_dir_data {
    uint64_t    block;
    uint16_t    offset;
};

static lba_t block_to_sector(struct filesystem *fs, uint64_t block)
{
    struct folifs_data *data = (struct folifs_data *)fs->data;

    return data->reserved_sectors + block * data->sectors_per_block;
}

static status_t read_block(struct filesystem *fs, uint64_t block)
{
    struct folifs_data *data = (struct folifs_data *)fs->data;
    status_t status;
    lba_t lba;

    lba = block_to_sector(fs, block);
    if (data->blkbuf_num == lba) return STATUS_SUCCESS;

    status = data->blkif->read(data->blkdev, lba, data->blkbuf, data->sectors_per_block, NULL);
    if (!CHECK_SUCCESS(status)) return status;

    data->blkbuf_num = block;

    return STATUS_SUCCESS;
}

static status_t probe(struct device *dev, struct fs_driver *drv);
static status_t mount(struct filesystem **fsout, struct fs_driver *drv, struct device *dev, const char *name);
static status_t unmount(struct filesystem *fs);

static status_t open(struct fs_directory *dir, const char *name, struct fs_file **fileout);
static status_t read(struct fs_file *file, void *buf, size_t len, size_t *result);
static status_t seek(struct fs_file *file, off_t offset, int origin);
static status_t tell(struct fs_file *file, off_t *result);
static void close(struct fs_file *file);

static status_t open_root_directory(struct filesystem *fs, struct fs_directory **dirout);
static status_t open_directory(struct fs_directory *dir, const char *name, struct fs_directory **dirout);
static status_t rewind_directory(struct fs_directory *dir);
static status_t iter_directory(struct fs_directory *dir, struct fs_directory_entry *entry);
static void close_directory(struct fs_directory *dir);

static void folifs_init(void)
{
    status_t status;
    struct fs_driver *drv;

    status = filesystem_driver_create(&drv);
    if (!CHECK_SUCCESS(status)) {
        panic(status, "cannot register fs driver \"folifs\"");
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

static status_t probe(struct device *dev, struct fs_driver *drv)
{
    status_t status;
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

    if (strncmp(lba0.filesystem_signature, FOLIFS_FS_SIGNATURE, sizeof(lba0.filesystem_signature)) != 0) {
        return STATUS_INVALID_SIGNATURE;
    }

    return STATUS_SUCCESS;
}

static status_t mount(struct filesystem **fsout, struct fs_driver *drv, struct device *dev, const char *name)
{
    status_t status;
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

    status = filesystem_create(&fs, drv, dev, name);
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

    if ((strncmp(lba0.filesystem_signature, FOLIFS_FS_SIGNATURE, sizeof(lba0.filesystem_signature)) != 0)) {
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
        filesystem_remove(fs);
    }

    return status;
}

static status_t unmount(struct filesystem *fs)
{
    struct folifs_data *data = (struct folifs_data *)fs->data;

    free(data->blkbuf);

    free(data);
    
    filesystem_remove(fs);

    return STATUS_SUCCESS;
}

static status_t open(struct fs_directory *dir, const char *name, struct fs_file **fileout)
{
    return STATUS_UNIMPLEMENTED;
}

static status_t read(struct fs_file *file, void *buf, size_t len, size_t *result)
{
    return STATUS_UNIMPLEMENTED;
}

static status_t seek(struct fs_file *file, off_t offset, int origin)
{
    return STATUS_UNIMPLEMENTED;
}

static status_t tell(struct fs_file *file, off_t *result)
{
    return STATUS_UNIMPLEMENTED;
}

static void close(struct fs_file *file)
{

}

static status_t open_root_directory(struct filesystem *fs, struct fs_directory **dirout)
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

    return STATUS_UNIMPLEMENTED;
}

static status_t open_directory(struct fs_directory *dir, const char *name, struct fs_directory **dirout)
{
    return STATUS_UNIMPLEMENTED;
}

static status_t rewind_directory(struct fs_directory *dir)
{
    return STATUS_UNIMPLEMENTED;
}

static status_t iter_directory(struct fs_directory *dir, struct fs_directory_entry *entry)
{
    return STATUS_UNIMPLEMENTED;
}

static void close_directory(struct fs_directory *dir)
{
    
}

REGISTER_FILESYSTEM_DRIVER(folifs, folifs_init)
