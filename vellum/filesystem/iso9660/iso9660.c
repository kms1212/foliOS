#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include <vellum/compiler.h>
#include <vellum/device.h>
#include <vellum/filesystem.h>
#include <vellum/interface/block.h>

#include "iso9660.h"

struct iso9660_dir_data {
    struct iso9660_dir_entry_header direntry;
    lba_t data_start_lba;
    lba_t current_lba;
    unsigned int current_entry_offset;
    size_t prev_entry_size;
};

struct iso9660_file_data {
    struct iso9660_dir_entry_header direntry;
    size_t cursor;
    lba_t data_start_lba;
};

struct iso9660_data {
    struct device *blkdev;
    const struct block_interface *blkif;

    uint8_t databuf[2048];
    lba_t databuf_lba;

    lba_t primary_descriptor_lba;
    lba_t pathtbl_lba;
    lba_t optional_pathtbl_lba;
    lba_t rootdir_lba;
    uint32_t pathtbl_size;
    uint32_t volume_sector_count;
    uint16_t sector_size;
};

static status_t read_sector(struct filesystem *fs, lba_t lba)
{
    struct iso9660_data *data = (struct iso9660_data *)fs->data;
    status_t status;

    if (data->databuf_lba == lba) return STATUS_SUCCESS;

    status = data->blkif->read(data->blkdev, lba, data->databuf, 1, NULL);
    if (!CHECK_SUCCESS(status)) return status;

    data->databuf_lba = lba;

    return STATUS_SUCCESS;
}

static status_t match_name(
    struct fs_directory *dir, const char *name, struct fs_directory_entry *direntry
)
{
    status_t status;
    struct filesystem *fs = dir->fs;

    status = fs->driver->rewind_directory(dir);
    if (!CHECK_SUCCESS(status)) return status;

    while (!fs->driver->iter_directory(dir, direntry)) {
        if (strncasecmp(name, direntry->name, sizeof(direntry->name)) == 0) {
            return STATUS_SUCCESS;
        }
    }

    return STATUS_ENTRY_NOT_FOUND;
}

static status_t probe(struct device *dev, struct fs_driver *drv);
static status_t mount(
    struct filesystem **fsout, struct fs_driver *drv, struct device *dev, const char *name
);
static status_t unmount(struct filesystem *fs);

static status_t open(struct fs_directory *dir, const char *name, struct fs_file **fileout);
static status_t read(struct fs_file *file, void *buf, size_t len, size_t *result);
static status_t seek(struct fs_file *file, off_t offset, int origin);
static status_t tell(struct fs_file *file, off_t *result);
static void close(struct fs_file *file);

static status_t open_root_directory(struct filesystem *fs, struct fs_directory **dirout);
static status_t open_directory(
    struct fs_directory *dir, const char *name, struct fs_directory **dirout
);
static status_t rewind_directory(struct fs_directory *dir);
static status_t iter_directory(struct fs_directory *dir, struct fs_directory_entry *entry);
static void close_directory(struct fs_directory *dir);

static void iso9660_init(void)
{
    status_t status;
    struct fs_driver *drv;

    status = VlFs_CreateDriver(&drv);
    if (!CHECK_SUCCESS(status)) {
        VlP_Panic(status, "cannot register fs driver \"iso9660\"");
    }

    drv->name = "iso9660";
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
    lba_t current_descriptor_lba;
    lba_t primary_descriptor_lba = -1;
    struct iso9660_vol_desc voldesc;

    blkdev = dev;
    if (!blkdev) return STATUS_INVALID_VALUE;

    status = blkdev->driver->get_interface(blkdev, "block", (const void **)&blkif);
    if (!CHECK_SUCCESS(status)) return status;

    status = blkif->get_block_size(blkdev, &block_size);
    if (!CHECK_SUCCESS(status)) return status;
    if (block_size != 2048) return STATUS_SIZE_CHECK_FAILURE;

    /* find primary volume descriptor */
    current_descriptor_lba = 16;
    do {
        status = blkif->read(blkdev, current_descriptor_lba, &voldesc, 1, NULL);
        if (!CHECK_SUCCESS(status)) return status;

        /* check signature */
        if (strncmp(voldesc.signature, ISO9660_SIGNATURE, 5) != 0) {
            return STATUS_INVALID_SIGNATURE;
        }

        if (voldesc.type == VDTYPE_PRIVOLDESC && primary_descriptor_lba < 0) {
            primary_descriptor_lba = current_descriptor_lba;
        }
        current_descriptor_lba++;
    } while (voldesc.type != VDTYPE_VDSETTERM);

    if (primary_descriptor_lba < 0) return STATUS_ENTRY_NOT_FOUND;

    return STATUS_SUCCESS;
}

static status_t mount(
    struct filesystem **fsout, struct fs_driver *drv, struct device *dev, const char *name
)
{
    status_t status;
    struct filesystem *fs = NULL;
    struct device *blkdev = NULL;
    const struct block_interface *blkif = NULL;
    size_t block_size;
    struct iso9660_data *data = NULL;
    lba_t current_descriptor_lba;
    struct iso9660_vol_desc *voldesc = NULL;
    struct iso9660_pathtbl_entry_header *pathtbl_entry = NULL;

    blkdev = dev;
    if (!blkdev) {
        status = STATUS_INVALID_VALUE;
        goto has_error;
    }

    status = blkdev->driver->get_interface(blkdev, "block", (const void **)&blkif);
    if (!CHECK_SUCCESS(status)) goto has_error;

    status = blkif->get_block_size(blkdev, &block_size);
    if (!CHECK_SUCCESS(status)) return status;
    if (block_size != 2048) return STATUS_SIZE_CHECK_FAILURE;

    status = VlFs_Create(&fs, drv, dev, name);
    if (!CHECK_SUCCESS(status)) goto has_error;

    data = malloc(sizeof(*data));
    if (!data) {
        status = STATUS_UNKNOWN_ERROR;
        goto has_error;
    }

    data->blkdev = blkdev;
    data->blkif = blkif;
    data->databuf_lba = -1;
    data->primary_descriptor_lba = -1;
    fs->data = data;

    /* find primary volume descriptor */
    current_descriptor_lba = 16;
    do {
        status = read_sector(fs, current_descriptor_lba);
        if (!CHECK_SUCCESS(status)) goto has_error;

        voldesc = (void *)data->databuf;

        /* check signature */
        if (strncmp(voldesc->signature, ISO9660_SIGNATURE, 5) != 0) {
            status = STATUS_INVALID_SIGNATURE;
            goto has_error;
        }

        if (voldesc->type == VDTYPE_PRIVOLDESC && data->primary_descriptor_lba < 0) {
            data->primary_descriptor_lba = current_descriptor_lba;
        }
        current_descriptor_lba++;
    } while (voldesc->type != VDTYPE_VDSETTERM);

    if (data->primary_descriptor_lba < 0) {
        status = STATUS_ENTRY_NOT_FOUND;
        goto has_error;
    }

    status = read_sector(fs, data->primary_descriptor_lba);
    if (!CHECK_SUCCESS(status)) goto has_error;

    voldesc = (void *)data->databuf;

#if defined(__PROCESSOR_BIG_ENDIAN)
    data->pathtbl_lba = voldesc->pvd.lba_be_pathtbl;
    data->optional_pathtbl_lba = voldesc->pvd.lba_be_pathtbl_optional;

#else
    data->pathtbl_lba = voldesc->pvd.lba_le_pathtbl;
    data->optional_pathtbl_lba = voldesc->pvd.lba_le_pathtbl_optional;

#endif

    data->pathtbl_size = GET_BIENDIAN(&voldesc->pvd.pathtbl_size);
    data->sector_size = GET_BIENDIAN(&voldesc->pvd.sector_size);
    data->volume_sector_count = GET_BIENDIAN(&voldesc->pvd.vol_sector_count);

    status = read_sector(fs, data->pathtbl_lba);
    if (!CHECK_SUCCESS(status)) goto has_error;

    pathtbl_entry = (void *)data->databuf;
    data->rootdir_lba = pathtbl_entry->lba_data;

    return STATUS_SUCCESS;

has_error:
    if (data) {
        free(data);
    }

    if (fs) {
        VlFs_Remove(fs);
    }

    return status;
}

static status_t unmount(struct filesystem *fs)
{
    struct iso9660_data *data = (struct iso9660_data *)fs->data;

    free(data);

    VlFs_Remove(fs);

    return STATUS_SUCCESS;
}

static status_t open(struct fs_directory *dir, const char *name, struct fs_file **fileout)
{
    status_t status;
    struct filesystem *fs = dir->fs;
    struct iso9660_dir_data *dir_data = (struct iso9660_dir_data *)dir->data;
    struct fs_file *file = NULL;
    struct iso9660_file_data *file_data = NULL;
    struct fs_directory_entry dirent;

    if (!fileout) return STATUS_INVALID_VALUE;

    status = match_name(dir, name, &dirent);
    if (!CHECK_SUCCESS(status)) goto has_error;

    if (dir_data->direntry.directory) {
        status = STATUS_WRONG_ELEMENT_TYPE;
        goto has_error;
    }

    file = malloc(sizeof(*file));
    if (!file) {
        status = STATUS_UNKNOWN_ERROR;
        goto has_error;
    }
    file->fs = fs;

    file_data = malloc(sizeof(*file_data));
    if (!file_data) {
        status = STATUS_UNKNOWN_ERROR;
        goto has_error;
    }
    file_data->data_start_lba = GET_BIENDIAN(&dir_data->direntry.lba_data_location);
    file_data->cursor = 0;
    memcpy(&file_data->direntry, &dir_data->direntry, sizeof(file_data->direntry));
    file->data = file_data;

    *fileout = file;

    return STATUS_SUCCESS;

has_error:
    if (file) {
        free(file);
    }

    if (file_data) {
        free(file_data);
    }

    return status;
}

static status_t read(struct fs_file *file, void *buf, size_t len, size_t *result)
{
    status_t status;
    struct filesystem *fs = file->fs;
    struct iso9660_data *data = (struct iso9660_data *)fs->data;
    struct iso9660_file_data *file_data = (struct iso9660_file_data *)file->data;
    size_t total_read_len = 0;
    size_t file_size = GET_BIENDIAN(&file_data->direntry.data_size);

    if (file_data->cursor >= file_size) {
        return STATUS_END_OF_FILE;
    }

    if (file_data->cursor + len >= file_size) {
        len = file_size - file_data->cursor;
    }

    while (len > 0) {
        uint32_t sector_offset = file_data->cursor % data->sector_size;
        uint32_t read_len = data->sector_size - sector_offset;
        if (read_len > len) {
            read_len = len;
        }

        status = read_sector(fs, file_data->data_start_lba + file_data->cursor / data->sector_size);
        if (!CHECK_SUCCESS(status)) return status;

        memcpy(buf, data->databuf + sector_offset, read_len);
        len -= read_len;
        buf = (uint8_t *)buf + read_len;
        total_read_len += read_len;
        file_data->cursor += read_len;
    }

    if (result) *result = total_read_len;

    return STATUS_SUCCESS;
}

static status_t seek(struct fs_file *file, off_t offset, int origin)
{
    struct iso9660_file_data *file_data = (struct iso9660_file_data *)file->data;
    int64_t new_cursor;
    size_t file_size = GET_BIENDIAN(&file_data->direntry.data_size);

    switch (origin) {
    case SEEK_SET:
        new_cursor = offset;
        break;
    case SEEK_CUR:
        new_cursor = file_data->cursor + offset;
        break;
    case SEEK_END:
        new_cursor = file_size + offset;
        break;
    default:
        return STATUS_INVALID_VALUE;
    }
    if (new_cursor < 0) return STATUS_INVALID_VALUE;
    if (new_cursor > file_size) {
        new_cursor = file_size;
    }

    file_data->cursor = new_cursor;

    return STATUS_SUCCESS;
}

static status_t tell(struct fs_file *file, off_t *result)
{
    struct iso9660_file_data *file_data = (struct iso9660_file_data *)file->data;

    if (result) *result = file_data->cursor;

    return STATUS_SUCCESS;
}

static void close(struct fs_file *file)
{
    struct iso9660_file_data *file_data = (struct iso9660_file_data *)file->data;

    free(file_data);

    free(file);
}

static status_t open_root_directory(struct filesystem *fs, struct fs_directory **dirout)
{
    status_t status;
    struct iso9660_data *data = (struct iso9660_data *)fs->data;
    struct fs_directory *dir = NULL;
    struct iso9660_dir_data *dir_data = NULL;

    if (!dirout) return STATUS_INVALID_VALUE;

    dir = malloc(sizeof(*dir));
    if (!dir) {
        status = STATUS_UNKNOWN_ERROR;
        goto has_error;
    }

    dir->fs = fs;

    dir_data = malloc(sizeof(*dir_data));
    if (!dir_data) {
        status = STATUS_UNKNOWN_ERROR;
        goto has_error;
    }

    dir_data->current_entry_offset = 0;
    dir_data->prev_entry_size = 0;
    dir_data->data_start_lba = dir_data->current_lba = data->rootdir_lba;
    dir->data = dir_data;

    memset(&dir_data->direntry, 0, sizeof(dir_data->direntry));

    *dirout = dir;

    return STATUS_SUCCESS;

has_error:
    if (dir) {
        free(dir);
    }

    if (dir_data) {
        free(dir_data);
    }

    return status;
}

static status_t open_directory(
    struct fs_directory *dir, const char *name, struct fs_directory **dirout
)
{
    status_t status;
    struct filesystem *fs = dir->fs;
    struct iso9660_data *data = (struct iso9660_data *)fs->data;
    struct iso9660_dir_data *dir_data = (struct iso9660_dir_data *)dir->data;
    struct fs_directory *new_dir = NULL;
    struct iso9660_dir_data *new_dir_data = NULL;
    struct fs_directory_entry dirent;

    if (dir_data->data_start_lba == data->rootdir_lba &&
        (strcmp(".", name) == 0 || strcmp("..", name) == 0)) {
        return open_root_directory(fs, dirout);
    }

    if (!dirout) return STATUS_INVALID_VALUE;

    status = match_name(dir, name, &dirent);
    if (!CHECK_SUCCESS(status)) goto has_error;

    if (!dir_data->direntry.directory) {
        status = STATUS_WRONG_ELEMENT_TYPE;
        goto has_error;
    }

    new_dir = malloc(sizeof(*dir));
    if (!new_dir) {
        status = STATUS_UNKNOWN_ERROR;
        goto has_error;
    }
    new_dir->fs = fs;

    new_dir_data = malloc(sizeof(*new_dir_data));
    if (!new_dir_data) {
        status = STATUS_UNKNOWN_ERROR;
        goto has_error;
    }
    new_dir_data->current_entry_offset = 0;
    new_dir_data->prev_entry_size = 0;
    new_dir_data->data_start_lba = new_dir_data->current_lba =
        GET_BIENDIAN(&dir_data->direntry.lba_data_location);
    memset(&new_dir_data->direntry, 0, sizeof(new_dir_data->direntry));
    new_dir->data = new_dir_data;

    *dirout = new_dir;

    return STATUS_SUCCESS;

has_error:
    if (new_dir) {
        free(new_dir);
    }

    if (new_dir_data) {
        free(new_dir_data);
    }

    return status;
}

static status_t rewind_directory(struct fs_directory *dir)
{
    struct iso9660_dir_data *dir_data = (struct iso9660_dir_data *)dir->data;

    dir_data->current_lba = dir_data->data_start_lba;
    dir_data->current_entry_offset = 0;
    memset(&dir_data->direntry, 0, sizeof(dir_data->direntry));

    return STATUS_SUCCESS;
}

static status_t iter_directory(struct fs_directory *dir, struct fs_directory_entry *entry)
{
    status_t status;
    struct filesystem *fs = dir->fs;
    struct iso9660_data *data = (struct iso9660_data *)fs->data;
    struct iso9660_dir_data *dir_data = (struct iso9660_dir_data *)dir->data;
    struct iso9660_dir_entry_header *dirent_header = NULL;
    const char *filename;
    size_t filename_len;

    status = read_sector(fs, dir_data->current_lba);
    if (!CHECK_SUCCESS(status)) return status;

    dirent_header = (void *)((uintptr_t)data->databuf + dir_data->current_entry_offset);
    if (!dirent_header->entry_size) return STATUS_END_OF_LIST;

    dir_data->prev_entry_size = dirent_header->entry_size + dirent_header->attrib_record_size;
    dir_data->current_entry_offset += dirent_header->entry_size + dirent_header->attrib_record_size;
    if (dir_data->current_entry_offset >= data->sector_size) {
        dir_data->current_entry_offset = 0;
        dir_data->data_start_lba++;
    }

    filename = (const char *)((uintptr_t)dirent_header + sizeof(*dirent_header));
    if (!filename[0] || dirent_header->filename_len == 0) {
        strncpy(entry->name, ".", 1);
        entry->name[1] = '\0';
    } else if (filename[0] == '\1') {
        strncpy(entry->name, "..", 2);
        entry->name[2] = '\0';
    } else {
        filename_len = dirent_header->filename_len;
        for (size_t i = 0; i < dirent_header->filename_len; i++) {
            if (filename[i] == ';') {
                filename_len = i;
                break;
            }
        }

        strncpy(entry->name, filename, filename_len);
        entry->name[filename_len] = '\0';
    }
    entry->size = GET_BIENDIAN(&dirent_header->data_size);

    memcpy(&dir_data->direntry, dirent_header, sizeof(dir_data->direntry));

    return STATUS_SUCCESS;
}

static void close_directory(struct fs_directory *dir)
{
    struct iso9660_dir_data *dir_data = (struct iso9660_dir_data *)dir->data;

    free(dir_data);

    free(dir);
}

REGISTER_FILESYSTEM_DRIVER(iso9660, iso9660_init)
