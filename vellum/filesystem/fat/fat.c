#include <ctype.h>
#include <endian.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include <vellum/compiler.h>
#include <vellum/device.h>
#include <vellum/disk.h>
#include <vellum/filesystem.h>
#include <vellum/interface/block.h>
#include <vellum/status.h>

#include "fat.h"

struct fat_dir_data {
    fatcluster_t head_cluster, current_cluster;
    int root_cluster;
    unsigned int current_entry_index;
    struct fat_direntry_file direntry;
};

struct fat_file_data {
    fatcluster_t direntry_cluster;
    unsigned int direntry_entry_index;
    struct fat_direntry_file direntry;

    fatcluster_t head_cluster, current_cluster;
    uint32_t cursor;
};

struct fat_data {
    struct device *blkdev;
    const struct block_interface *blkif;

    uint16_t reserved_sectors;
    uint16_t sector_size;
    uint32_t cluster_size;
    uint8_t sectors_per_cluster;
    uint8_t fat_type;
    uint32_t data_area_begin;
    uint32_t fat_size;
    uint32_t free_clusters;
    uint32_t next_free_cluster;
    uint32_t total_sector_count;
    uint32_t root_cluster;
    uint16_t root_entry_count;
    uint16_t root_sector_count;
    uint16_t fsinfo_sector;

    lba_t fatbuf_lba;
    uint8_t fatbuf[FAT_SECTOR_SIZE];
    lba_t databuf_lba_start;
    uint8_t *databuf;

    fatcluster_t (*sector_to_cluster)(struct filesystem *, lba_t);
    lba_t (*cluster_to_sector)(struct filesystem *, fatcluster_t);
    status_t (*get_next_cluster)(struct filesystem *, fatcluster_t *, unsigned int);
};

static size_t remove_right_padding(char *dest, const char *orig, size_t dest_len, size_t orig_len)
{
    int str_len = 0;
    for (size_t cur = orig_len - 1; cur >= 0; cur--) {
        if (str_len) {
            dest[cur] = orig[cur];
            str_len++;
        } else if (orig[cur] != ' ') {
            str_len++;
            dest[cur] = orig[cur];
            if (cur < dest_len) {
                dest[cur + 1] = 0;
            }
        }
    }
    return str_len;
}

static int validate_sfn(const char *str, size_t len)
{
    /*  Characters Allowed:
        - A-Z
        - 0-9
        - char > 127
        - (space) $ % - _ @ ~ ` ! ( ) { } ^ # &

        Invalid Names:
        - .
        - ..

        Reference: https://averstak.tripod.com/fatdox/names.htm
     */
    static const uint32_t bitmap[] = {
        0x00000000,
        0x03FF237B, /* ASCII 0x00 - 0x3F */
        0xC3FFFFFF,
        0x68000001, /* ASCII 0x40 - 0x7F */
    };

    int has_dot = 0;

    if (len > 0 && str[0] == '.') {
        return 0;
    }

    for (int i = 0; str[i] != 0 && i < len; i++) {
        if (str[i] > 0x7F) {
            continue;
        } else if (str[i] == '.') {
            if (has_dot) {
                return 0;
            }
            has_dot = 1;
        }
        const uint32_t bmval = bitmap[str[i] >> 5];
        if (!((bmval >> (str[i] & 31)) & 1)) {
            return 0;
        }
    }
    return 1;
}

static int validate_lfn(const char *str, size_t len)
{
    /*  Characters Not Allowed:
        - \ / : * ? " < > |
        - Control characters

        Invalid Names:
        - .
        - ..

        Reference: https://en.wikipedia.org/wiki/Long_filename
     */
    static const uint32_t bitmap[] = {
        0x00000000,
        0x23FF7BFB, /* ASCII 0x00 - 0x3F */
        0xFFFFFFFF,
        0x6FFFFFFF, /* ASCII 0x40 - 0x7F */
    };

    if (len > 0 && str[0] == '.') {
        return 0;
    }

    for (int i = 0; str[i] != 0 && i < len; i++) {
        if (i > 0x7F) {
            continue;
        }
        const uint32_t bmval = bitmap[str[i] >> 5];
        if (!((bmval >> (str[i] & 31)) & 1)) {
            return 0;
        }
    }
    return 1;
}

static int ucs2_to_utf8(char *buf, size_t len, uint16_t ucs2ch)
{
    if (ucs2ch == 0 || ucs2ch == 0xFFFF) return 0;

    if (ucs2ch < 0x7F) {
        if (len < 1) return -1;
        *buf = (char)ucs2ch;
        return 1;
    }

    if (ucs2ch < 0x7FF) {
        if (len < 2) return -1;
        *buf++ = (char)(((ucs2ch & 0x07C0) >> 6) | 0xC0);
        *buf++ = (char)((ucs2ch & 0x003F) | 0x80);
        return 2;
    }

    if (len < 3) return -1;
    *buf++ = (char)(((ucs2ch & 0xF000) >> 12) | 0xE0);
    *buf++ = (char)(((ucs2ch & 0x0FC0) >> 6) | 0x80);
    *buf++ = (char)((ucs2ch & 0x003F) | 0x80);
    return 3;
}

static size_t get_lfn_filename(
    const struct fat_direntry_lfn *entry, uint16_t buf[static FAT_LFN_BUFLEN]
)
{
    buf += ((entry->sequence_index & 0x1F) - 1) * 13;
    size_t char_count = 0;
    for (int i = 0; i < 5; i++) {
        buf[char_count++] = entry->name_fragment1[i];
    }
    for (int i = 0; i < 6; i++) {
        buf[char_count++] = entry->name_fragment2[i];
    }
    for (int i = 0; i < 2; i++) {
        buf[char_count++] = entry->name_fragment3[i];
    }
    if (entry->sequence_index & 0x40) {
        buf[char_count] = '\0';
    }
    return char_count;
}

static size_t lfn_ucs2_to_utf8(
    char utf8buf[static FAT_LFN_U8_BUFLEN],
    const uint16_t ucs2buf[static FAT_LFN_BUFLEN],
    int allow_nonascii,
    char fallback
)
{
    size_t bytes_written = 0;
    if (allow_nonascii) {
        size_t buflen = FAT_LFN_U8_BUFLEN;
        while (bytes_written < FAT_LFN_U8_BUFLEN) {
            int u8ch_len = ucs2_to_utf8(utf8buf, buflen, *ucs2buf++);
            if (!u8ch_len) {
                *utf8buf = 0;
                break;
            } else if (u8ch_len < 0) {
                *utf8buf = fallback;
                u8ch_len = 1;
            }
            utf8buf += u8ch_len;
            bytes_written += u8ch_len;
            buflen -= u8ch_len;
        }
    } else {
        while (bytes_written < FAT_LFN_BUFLEN) {
            *utf8buf++ = (char)(*ucs2buf < 0x80 ? *ucs2buf : fallback);
            ucs2buf++;
            bytes_written++;
        }
        *utf8buf = 0;
    }

    return bytes_written;
}

static size_t get_sfn_filename(const struct fat_direntry_file *entry, char *buf)
{
    size_t char_count = 0;

    for (int i = 0; i < 8 && entry->name[i] != ' '; i++) {
        buf[char_count++] =
            (char)((entry->attribute2 & FAT_ATTR2_LCASE_NAME) ? tolower(entry->name[i])
                                                              : entry->name[i]);
    }
    if (entry->extension[0] != ' ') {
        buf[char_count++] = '.';
    }
    for (int i = 0; i < 3 && entry->extension[i] != ' '; i++) {
        buf[char_count++] =
            (char)((entry->attribute2 & FAT_ATTR2_LCASE_EXT) ? tolower(entry->extension[i])
                                                             : entry->extension[i]);
    }
    buf[char_count] = '\0';

    return char_count;
}

static uint8_t get_sfn_checksum(char buf[static FAT_SFN_BUFLEN])
{
    uint8_t chksum = 0;

    for (int i = FAT_SFN_BUFLEN; i != 0; i--) {
        chksum = ((chksum & 1) ? 0x80 : 0) + (chksum >> 1) + *buf++;
    }

    return chksum;
}

static status_t read_fat(struct filesystem *fs, uint32_t fat_sector)
{
    struct fat_data *data = (struct fat_data *)fs->data;
    status_t status;
    lba_t lba;

    lba = data->reserved_sectors + fat_sector;
    if (data->fatbuf_lba == lba) return STATUS_SUCCESS;

    status = data->blkif->read(data->blkdev, lba, data->fatbuf, 1, NULL);
    if (!CHECK_SUCCESS(status)) return status;

    data->fatbuf_lba = lba;

    return STATUS_SUCCESS;
}

static fatcluster_t sector_to_cluster12_16(struct filesystem *fs, lba_t lba)
{
    struct fat_data *data = (struct fat_data *)fs->data;

    return (lba - data->data_area_begin - data->root_sector_count) / data->sectors_per_cluster + 2;
}

static fatcluster_t sector_to_cluster32(struct filesystem *fs, lba_t lba)
{
    struct fat_data *data = (struct fat_data *)fs->data;

    return (lba - data->data_area_begin) / data->sectors_per_cluster + 2;
}

static fatcluster_t sector_to_cluster(struct filesystem *fs, lba_t lba)
{
    struct fat_data *data = (struct fat_data *)fs->data;

    return data->sector_to_cluster(fs, lba);
}

static lba_t fatcluster_to_sector12_16(struct filesystem *fs, fatcluster_t cluster)
{
    struct fat_data *data = (struct fat_data *)fs->data;

    return ((cluster - 2) * data->sectors_per_cluster) + data->data_area_begin +
        data->root_sector_count;
}

static lba_t fatcluster_to_sector32(struct filesystem *fs, fatcluster_t cluster)
{
    struct fat_data *data = (struct fat_data *)fs->data;

    return ((cluster - 2) * data->sectors_per_cluster) + data->data_area_begin;
}

static lba_t fatcluster_to_sector(struct filesystem *fs, fatcluster_t cluster)
{
    struct fat_data *data = (struct fat_data *)fs->data;

    return data->cluster_to_sector(fs, cluster);
}

static status_t read_sector(struct filesystem *fs, lba_t lba)
{
    struct fat_data *data = (struct fat_data *)fs->data;
    status_t status;

    if (data->databuf_lba_start == lba) return STATUS_SUCCESS;

    status = data->blkif->read(data->blkdev, lba, data->databuf, 1, NULL);
    if (!CHECK_SUCCESS(status)) return status;

    data->databuf_lba_start = lba;

    return STATUS_SUCCESS;
}

static status_t read_cluster(struct filesystem *fs, fatcluster_t cluster)
{
    struct fat_data *data = (struct fat_data *)fs->data;
    status_t status;
    lba_t lba;

    lba = fatcluster_to_sector(fs, cluster);
    if (data->databuf_lba_start == lba) return STATUS_SUCCESS;

    status = data->blkif->read(data->blkdev, lba, data->databuf, data->sectors_per_cluster, NULL);
    if (!CHECK_SUCCESS(status)) return status;

    data->databuf_lba_start = lba;

    return STATUS_SUCCESS;
}

static status_t get_next_cluster12(struct filesystem *fs, fatcluster_t *cluster, unsigned int count)
{
    status_t status;
    struct fat_data *data = (struct fat_data *)fs->data;
    uint16_t byte_idx;
    uint16_t sector_idx;
    uint8_t fatentry_buf[2];

    if (*cluster > FAT12_MAX_CLUSTER) return STATUS_INVALID_VALUE;

    while (count-- > 0) {
        byte_idx = *cluster + (*cluster >> 1);
        sector_idx = byte_idx / data->sector_size;
        byte_idx %= data->sector_size;

        status = read_fat(fs, sector_idx);
        if (!CHECK_SUCCESS(status)) return status;

        fatentry_buf[0] = data->fatbuf[byte_idx];
        if (byte_idx == data->sector_size - 1) {
            status = read_fat(fs, sector_idx + 1);
            if (!CHECK_SUCCESS(status)) return status;
            fatentry_buf[1] = data->fatbuf[0];
        } else {
            fatentry_buf[1] = data->fatbuf[byte_idx + 1];
        }

        if (*cluster & 1) { /* odd-numbered cluster */
            *cluster = ((fatentry_buf[0] & 0xF0) >> 4) | (fatentry_buf[1] << 4);
        } else { /* even-numbered cluster */
            *cluster = fatentry_buf[0] | ((fatentry_buf[1] & 0x0F) << 8);
        }

        if (*cluster > FAT12_MAX_CLUSTER) return STATUS_END_OF_LIST;
    }

    return STATUS_SUCCESS;
}

static status_t get_next_cluster16(struct filesystem *fs, fatcluster_t *cluster, unsigned int count)
{
    status_t status;
    struct fat_data *data = (struct fat_data *)fs->data;
    uint32_t fatentry_idx;
    uint32_t sector_idx;

    if (*cluster > FAT16_MAX_CLUSTER) return STATUS_INVALID_VALUE;

    while (count-- > 0) {
        fatentry_idx = *cluster;
        sector_idx = fatentry_idx / (data->sector_size >> 1);
        fatentry_idx %= data->sector_size >> 1;

        status = read_fat(fs, sector_idx);
        if (!CHECK_SUCCESS(status)) return status;

        *cluster = ((uint16_t *)data->fatbuf)[fatentry_idx];

        if (*cluster > FAT16_MAX_CLUSTER) return STATUS_END_OF_LIST;
    }

    return STATUS_SUCCESS;
}

static status_t get_next_cluster32(struct filesystem *fs, fatcluster_t *cluster, unsigned int count)
{
    status_t status;
    struct fat_data *data = (struct fat_data *)fs->data;
    uint32_t fatentry_idx;
    uint32_t sector_idx;

    if (*cluster > FAT32_MAX_CLUSTER) return STATUS_INVALID_VALUE;

    while (count-- > 0) {
        fatentry_idx = *cluster;
        sector_idx = fatentry_idx / (data->sector_size >> 2);
        fatentry_idx %= data->sector_size >> 2;

        status = read_fat(fs, sector_idx);
        if (!CHECK_SUCCESS(status)) return status;

        *cluster = ((uint32_t *)data->fatbuf)[fatentry_idx] & 0x0FFFFFFF;
        if (*cluster > FAT32_MAX_CLUSTER) return STATUS_END_OF_LIST;
    }

    return STATUS_SUCCESS;
}

static status_t get_next_cluster(struct filesystem *fs, fatcluster_t *cluster, unsigned int count)
{
    struct fat_data *data = (struct fat_data *)fs->data;

    return data->get_next_cluster(fs, cluster, count);
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

static void fat_init(void)
{
    status_t status;
    struct fs_driver *drv;

    status = VlFs_CreateDriver(&drv);
    if (!CHECK_SUCCESS(status)) {
        VlP_Panic(status, "cannot register fs driver \"fat\"");
    }

    drv->name = "fat";
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
    struct fat_bpb_sector bpb;
    struct fat_fsinfo fsinfo;
    unsigned int sector_size, sectors_per_cluster, fsinfo_sector;
    unsigned int root_entry_count, reserved_sectors, root_sector_count;
    unsigned int fat_size, total_sector_count, data_area_begin, cluster_count;

    blkdev = dev;
    if (!blkdev) return STATUS_INVALID_VALUE;

    status = blkdev->driver->get_interface(blkdev, "block", (const void **)&blkif);
    if (!CHECK_SUCCESS(status)) return status;

    status = blkif->get_block_size(blkdev, &block_size);
    if (!CHECK_SUCCESS(status)) return status;
    if (block_size != 512) return STATUS_SIZE_CHECK_FAILURE;

    /* read sector 0 (BPB) */
    status = blkif->read(blkdev, 0, &bpb, 1, NULL);
    if (!CHECK_SUCCESS(status)) return status;

    /* check signatures */
    if (le16toh(bpb.signature) != FAT_BPB_SIGNATURE) return STATUS_INVALID_SIGNATURE;
    if ((strncmp(bpb.fat.fs_type, FAT12_FSTYPE_STR, sizeof(bpb.fat.fs_type)) != 0) &&
        (strncmp(bpb.fat.fs_type, FAT16_FSTYPE_STR, sizeof(bpb.fat.fs_type)) != 0) &&
        (strncmp(bpb.fat32.fs_type, FAT32_FSTYPE_STR, sizeof(bpb.fat32.fs_type)) != 0)) {
        return STATUS_INVALID_SIGNATURE;
    }

    sector_size = le16toh(bpb.bytes_per_sector);
    sectors_per_cluster = bpb.sectors_per_cluster;
    root_entry_count = le16toh(bpb.root_entry_count);
    reserved_sectors = le16toh(bpb.reserved_sector_count);
    root_sector_count = ((root_entry_count * 32) + (sector_size - 1)) / sector_size;
    fat_size = bpb.fat_size16 ? le16toh(bpb.fat_size16) : le32toh(bpb.fat32.fat_size32);
    total_sector_count = bpb.total_sector_count16 ? le16toh(bpb.total_sector_count16)
                                                  : le32toh(bpb.total_sector_count32);
    data_area_begin = reserved_sectors + (bpb.fat_count * fat_size);
    cluster_count =
        (total_sector_count - data_area_begin - root_sector_count) / sectors_per_cluster;

    if (cluster_count > FAT16_MAX_CLUSTER) { /* FAT32 */
        fsinfo_sector = bpb.fat32.fsinfo_sector;

        status = blkif->read(blkdev, fsinfo_sector, &fsinfo, 1, NULL);
        if (!CHECK_SUCCESS(status)) return status;

        if ((le32toh(fsinfo.signature1) != FAT_FSINFO_SIGNATURE1) ||
            (le32toh(fsinfo.signature2) != FAT_FSINFO_SIGNATURE2) ||
            (le32toh(fsinfo.signature3) != FAT_FSINFO_SIGNATURE3)) {
            return STATUS_INVALID_SIGNATURE;
        }
    }

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
    struct fat_data *data = NULL;
    struct fat_bpb_sector bpb;
    struct fat_fsinfo fsinfo;

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
    data->databuf = NULL;
    data->databuf_lba_start = -1;
    fs->data = data;

    /* read sector 0 (BPB) */
    status = blkif->read(blkdev, 0, &bpb, 1, NULL);
    if (!CHECK_SUCCESS(status)) goto has_error;

    /* check signatures */
    if (le16toh(bpb.signature) != FAT_BPB_SIGNATURE) {
        status = STATUS_INVALID_SIGNATURE;
        goto has_error;
    }
    if ((strncmp(bpb.fat.fs_type, FAT12_FSTYPE_STR, sizeof(bpb.fat.fs_type)) != 0) &&
        (strncmp(bpb.fat.fs_type, FAT16_FSTYPE_STR, sizeof(bpb.fat.fs_type)) != 0) &&
        (strncmp(bpb.fat32.fs_type, FAT32_FSTYPE_STR, sizeof(bpb.fat32.fs_type)) != 0)) {
        status = STATUS_INVALID_SIGNATURE;
        goto has_error;
    }

    /* get information */
    data->sector_size = bpb.bytes_per_sector;
    data->sectors_per_cluster = bpb.sectors_per_cluster;
    data->cluster_size = data->sector_size * data->sectors_per_cluster;
    data->root_entry_count = bpb.root_entry_count;
    data->reserved_sectors = bpb.reserved_sector_count;
    data->root_sector_count =
        ((data->root_entry_count * 32) + (data->sector_size - 1)) / data->sector_size;
    data->fat_size = bpb.fat_size16 ? bpb.fat_size16 : bpb.fat32.fat_size32;
    data->total_sector_count =
        bpb.total_sector_count16 ? bpb.total_sector_count16 : bpb.total_sector_count32;
    data->data_area_begin = data->reserved_sectors + (bpb.fat_count * data->fat_size);
    uint32_t data_sectors =
        data->total_sector_count - data->data_area_begin - data->root_sector_count;
    uint32_t cluster_count = data_sectors / data->sectors_per_cluster;

    data->databuf = malloc(data->cluster_size);
    if (!data->databuf) {
        status = STATUS_UNKNOWN_ERROR;
        goto has_error;
    }

    /* dertermine FAT type */
    if (cluster_count <= FAT12_MAX_CLUSTER) {
        data->fat_type = FT_FAT12;
        data->cluster_to_sector = fatcluster_to_sector12_16;
        data->sector_to_cluster = sector_to_cluster12_16;
        data->get_next_cluster = get_next_cluster12;
    } else if (cluster_count <= FAT16_MAX_CLUSTER) {
        data->fat_type = FT_FAT16;
        data->cluster_to_sector = fatcluster_to_sector12_16;
        data->sector_to_cluster = sector_to_cluster12_16;
        data->get_next_cluster = get_next_cluster16;
    } else {
        data->fat_type = FT_FAT32;
        data->cluster_to_sector = fatcluster_to_sector32;
        data->sector_to_cluster = sector_to_cluster32;
        data->get_next_cluster = get_next_cluster32;
    }

    /* read FSINFO if FAT32 */
    if (data->fat_type == FT_FAT32) {
        data->root_cluster = bpb.fat32.root_cluster;
        data->fsinfo_sector = bpb.fat32.fsinfo_sector;

        status = blkif->read(blkdev, data->fsinfo_sector, &fsinfo, 1, NULL);
        if (!CHECK_SUCCESS(status)) return status;

        data->free_clusters = fsinfo.free_clusters;
        data->next_free_cluster = fsinfo.next_free_cluster;
    }

    return STATUS_SUCCESS;

has_error:
    if (data && data->databuf) {
        free(data->databuf);
    }

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
    struct fat_data *data = (struct fat_data *)fs->data;

    free(data->databuf);

    free(data);

    VlFs_Remove(fs);

    return STATUS_SUCCESS;
}

static status_t open(struct fs_directory *dir, const char *name, struct fs_file **fileout)
{
    struct filesystem *fs = dir->fs;
    struct fat_dir_data *dir_data = (struct fat_dir_data *)dir->data;
    status_t status;
    struct fs_directory_entry dirent;
    struct fat_file_data *file_data = NULL;
    struct fs_file *file = NULL;

    if (!fileout) return STATUS_INVALID_VALUE;

    status = match_name(dir, name, &dirent);
    if (!CHECK_SUCCESS(status)) goto has_error;

    if (dir_data->direntry.attribute & FAT_ATTR_DIRECTORY) {
        status = STATUS_WRONG_ELEMENT_TYPE;
        goto has_error;
    }

    file_data = malloc(sizeof(*file_data));
    if (!file_data) {
        status = STATUS_UNKNOWN_ERROR;
        goto has_error;
    }

    file_data->direntry_cluster = dir_data->current_cluster;
    file_data->direntry_entry_index = dir_data->current_entry_index;
    memcpy(&file_data->direntry, &dir_data->direntry, sizeof(dir_data->direntry));
    file_data->head_cluster =
        (dir_data->direntry.cluster_location_high << 16) | dir_data->direntry.cluster_location;
    file_data->current_cluster = file_data->head_cluster;
    file_data->cursor = 0;

    file = malloc(sizeof(*file));
    if (!file) {
        status = STATUS_UNKNOWN_ERROR;
        goto has_error;
    }

    file->fs = fs;
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
    struct filesystem *fs = file->fs;
    struct fat_data *data = (struct fat_data *)fs->data;
    struct fat_file_data *file_data = (struct fat_file_data *)file->data;
    status_t status;
    size_t total_read_len = 0;

    if (file_data->cursor >= file_data->direntry.size) {
        return STATUS_END_OF_FILE;
    }

    if (file_data->cursor + len >= file_data->direntry.size) {
        len = file_data->direntry.size - file_data->cursor;
    }

    while (len > 0) {
        uint32_t cluster_offset = file_data->cursor % data->cluster_size;
        uint32_t read_len = data->cluster_size - cluster_offset;
        if (read_len > len) {
            read_len = len;
        }

        status = read_cluster(fs, file_data->current_cluster);
        if (!CHECK_SUCCESS(status)) return status;

        memcpy(buf, data->databuf + cluster_offset, read_len);
        len -= read_len;
        buf = (uint8_t *)buf + read_len;
        total_read_len += read_len;
        file_data->cursor += read_len;

        if (cluster_offset + read_len < data->cluster_size) continue;

        status = get_next_cluster(fs, &file_data->current_cluster, 1);
        if (!CHECK_SUCCESS(status)) break;
    }

    if (result) *result = total_read_len;

    return STATUS_SUCCESS;
}

static status_t seek(struct fs_file *file, off_t offset, int origin)
{
    struct filesystem *fs = file->fs;
    struct fat_data *data = (struct fat_data *)fs->data;
    struct fat_file_data *file_data = (struct fat_file_data *)file->data;
    status_t status;
    int64_t new_cursor;
    fatcluster_t cluster;

    switch (origin) {
    case SEEK_SET:
        new_cursor = offset;
        break;
    case SEEK_CUR:
        new_cursor = file_data->cursor + offset;
        break;
    case SEEK_END:
        new_cursor = file_data->direntry.size + offset;
        break;
    default:
        return STATUS_INVALID_VALUE;
    }
    if (new_cursor < 0) return STATUS_INVALID_VALUE;
    if (new_cursor > file_data->direntry.size) {
        new_cursor = file_data->direntry.size;
    }

    cluster = file_data->head_cluster;
    status = get_next_cluster(fs, &cluster, new_cursor / data->cluster_size);
    if (!CHECK_SUCCESS(status)) return status;

    file_data->current_cluster = cluster;
    file_data->cursor = new_cursor;

    return STATUS_SUCCESS;
}

static status_t tell(struct fs_file *file, off_t *result)
{
    struct fat_file_data *file_data = (struct fat_file_data *)file->data;

    if (result) *result = file_data->cursor;

    return STATUS_SUCCESS;
}

static void close(struct fs_file *file)
{
    struct fat_file_data *file_data = (struct fat_file_data *)file->data;

    free(file_data);

    free(file);
}

static status_t open_root_directory(struct filesystem *fs, struct fs_directory **dirout)
{
    struct fat_data *data = (struct fat_data *)fs->data;
    status_t status;
    struct fat_dir_data *dir_data = NULL;
    struct fs_directory *dir = NULL;

    if (!dirout) return STATUS_INVALID_VALUE;

    dir_data = malloc(sizeof(*dir_data));
    if (!dir_data) {
        status = STATUS_UNKNOWN_ERROR;
        goto has_error;
    }

    dir_data->root_cluster = 1;
    dir_data->head_cluster = data->fat_type == FT_FAT32 ? data->root_cluster : 0;
    dir_data->current_cluster = dir_data->head_cluster;
    dir_data->current_entry_index = 0;

    dir = malloc(sizeof(*dir));
    if (!dir) {
        status = STATUS_UNKNOWN_ERROR;
        goto has_error;
    }

    dir->fs = fs;
    dir->data = dir_data;

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
    struct filesystem *fs = dir->fs;
    struct fat_data *data = (struct fat_data *)fs->data;
    struct fat_dir_data *dir_data = (struct fat_dir_data *)dir->data;
    status_t status;
    struct fs_directory *new_dir = NULL;
    struct fat_dir_data *new_dir_data = NULL;
    struct fs_directory_entry dirent;

    if (!dirout) return STATUS_INVALID_VALUE;

    if (dir_data->root_cluster && (strcmp(".", name) == 0 || strcmp("..", name) == 0)) {
        return open_root_directory(fs, dirout);
    }

    status = match_name(dir, name, &dirent);
    if (!CHECK_SUCCESS(status)) goto has_error;

    if (!(dir_data->direntry.attribute & FAT_ATTR_DIRECTORY)) {
        status = STATUS_WRONG_ELEMENT_TYPE;
        goto has_error;
    }

    new_dir_data = malloc(sizeof(*new_dir_data));
    if (!new_dir_data) {
        status = STATUS_UNKNOWN_ERROR;
        goto has_error;
    }

    if (dir_data->direntry.cluster_location == 0 && dir_data->direntry.cluster_location_high == 0) {
        new_dir_data->head_cluster = data->root_cluster;
        new_dir_data->root_cluster = 1;
    } else {
        new_dir_data->head_cluster =
            (dir_data->direntry.cluster_location_high << 16) | dir_data->direntry.cluster_location;
        new_dir_data->root_cluster = 0;
    }
    new_dir_data->current_cluster = new_dir_data->head_cluster;
    new_dir_data->current_entry_index = 0;

    new_dir = malloc(sizeof(*dir));
    if (!new_dir) {
        status = STATUS_UNKNOWN_ERROR;
        goto has_error;
    }
    new_dir->fs = fs;
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
    struct fat_dir_data *dir_data = (struct fat_dir_data *)dir->data;

    dir_data->current_cluster = dir_data->head_cluster;
    dir_data->current_entry_index = 0;
    memset(&dir_data->direntry, 0, sizeof(dir_data->direntry));

    return STATUS_SUCCESS;
}

static status_t iter_directory(struct fs_directory *dir, struct fs_directory_entry *entry)
{
    struct filesystem *fs = dir->fs;
    struct fat_data *data = (struct fat_data *)fs->data;
    struct fat_dir_data *dir_data = (struct fat_dir_data *)dir->data;

    status_t status;
    int is_rootdir = data->fat_type != FT_FAT32 && dir_data->head_cluster == 0;
    const uint16_t block_size = is_rootdir ? data->sector_size : data->cluster_size;
    uint16_t entries_per_block = block_size / sizeof(union fat_dir_entry);
    union fat_dir_entry *entries;
    int entry_found = 0;
    uint16_t lfn_ucs2_buf[FAT_LFN_BUFLEN];
    int is_lfn = 0;

    while (!entry_found) {
        /* fetch current block (sector or cluster) */
        if (is_rootdir) {
            /* root directory */
            if (dir_data->current_entry_index / entries_per_block >= data->root_sector_count) {
                return STATUS_END_OF_LIST;
            }
            status = read_sector(
                fs,
                data->data_area_begin + dir_data->current_entry_index / entries_per_block
            );
            if (!CHECK_SUCCESS(status)) return status;
        } else {
            fatcluster_t current_cluster = dir_data->current_cluster;
            if (dir_data->current_entry_index >= entries_per_block) {
                status = get_next_cluster(fs, &current_cluster, 1);
                if (!CHECK_SUCCESS(status)) return status;
                dir_data->current_entry_index = 0;
            }
            status = read_cluster(fs, current_cluster);
            if (!CHECK_SUCCESS(status)) return status;
            dir_data->current_cluster = current_cluster;
        }
        entries = (union fat_dir_entry *)data->databuf;

        while (is_rootdir || dir_data->current_entry_index < entries_per_block) {
            union fat_dir_entry *current_entry =
                &entries[dir_data->current_entry_index % entries_per_block];
            if ((uint8_t)current_entry->file.name[0] == 0) { /* End of entry list */
                return STATUS_END_OF_LIST;
            } else if ((uint8_t)current_entry->file.name[0] == 0xE5) {
                /* skip if file entry is deleted */
            } else if ((current_entry->file.attribute & FAT_ATTR_LFNENTRY) == FAT_ATTR_LFNENTRY) {
                /* write to LFN buffer */
                get_lfn_filename(&current_entry->lfn, lfn_ucs2_buf);
                is_lfn = 1;
            } else if (current_entry->file.attribute & FAT_ATTR_VOLUME_ID) {
                /* skip if file entry is volume id */
                is_lfn = 0;
            } else {
                entry_found = 1;
                break;
            }
            dir_data->current_entry_index++;
            if (!(dir_data->current_entry_index % entries_per_block)) {
                break;
            }
        }
    }

    if (is_lfn) {
        lfn_ucs2_to_utf8(entry->name, lfn_ucs2_buf, 1, '?');
    } else {
        get_sfn_filename(&entries[dir_data->current_entry_index].file, entry->name);
    }
    entry->size = entries[dir_data->current_entry_index].file.size;
    memcpy(
        &dir_data->direntry,
        &entries[dir_data->current_entry_index],
        sizeof(dir_data->direntry)
    );
    dir_data->current_entry_index++;

    return STATUS_SUCCESS;
}

static void close_directory(struct fs_directory *dir)
{
    struct fat_dir_data *dir_data = (struct fat_dir_data *)dir->data;

    free(dir_data);

    free(dir);
}

REGISTER_FILESYSTEM_DRIVER(fat, fat_init)
