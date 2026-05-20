#include "load_folios.h"

#include <inttypes.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <zlib.h>

#include <vellum/filesystem.h>
#include <vellum/log.h>
#include <vellum/macros.h>
#include <vellum/mm.h>
#include <vellum/path.h>
#include <vellum/plat/page.h>
#include <vellum/shell.h>

#include <stload/ramdisk.h>

struct ramdisk_builder {
    uint8_t *data;
    size_t size;
    size_t capacity;
};

struct ramdisk_build_entry {
    struct ramdisk_build_entry *next;
    struct fs_directory_entry dirent;
    struct fs_directory *dir;
    struct fs_file *file;
    size_t entry_offset;
    int is_directory;
};

static VlStatus ramdisk_reserve(struct ramdisk_builder *builder, size_t size, size_t *offset_out)
{
    size_t offset = builder->size;
    size_t new_size = ALIGN(builder->size + size, 4);

    if (new_size < builder->size) return STATUS_INVALID_VALUE;

    if (new_size > builder->capacity) {
        size_t new_capacity = builder->capacity ? builder->capacity : PAGE_SIZE;
        while (new_capacity < new_size) {
            size_t next_capacity = new_capacity * 2;
            if (next_capacity < new_capacity) return STATUS_INVALID_VALUE;
            new_capacity = next_capacity;
        }

        uint8_t *new_data = realloc(builder->data, new_capacity);
        if (!new_data) return STATUS_INSUFFICIENT_MEMORY;

        memset(new_data + builder->capacity, 0, new_capacity - builder->capacity);
        builder->data = new_data;
        builder->capacity = new_capacity;
    }

    memset(builder->data + offset, 0, new_size - offset);
    builder->size = new_size;

    if (offset_out) *offset_out = offset;

    return STATUS_SUCCESS;
}

static VlStatus open_directory_path(
    struct shell_instance *inst,
    const char *path,
    struct filesystem **fs_out,
    struct fs_directory **dir_out
)
{
    VlStatus status;
    struct filesystem *fs;
    struct fs_directory *dir = NULL;
    struct path_iterator iter;
    int iter_result;

    if (VlPath_IsAbsolute(path)) {
        VlPath_InitIter(&iter, path);

        if (iter.element[0]) {
            status = VlFs_Find(iter.element, &fs);
            if (!CHECK_SUCCESS(status)) return status;
        } else {
            fs = inst->fs;
            if (!fs) return STATUS_INVALID_VALUE;
        }

        status = fs->driver->open_root_directory(fs, &dir);
        if (!CHECK_SUCCESS(status)) return status;
    } else {
        fs = inst->fs;
        if (!fs || !inst->working_dir) return STATUS_INVALID_VALUE;

        VlPath_InitIter(&iter, path);
        dir = inst->working_dir;
    }

    do {
        iter_result = VlPath_AdvanceIter(&iter);
        if (iter.element[0] == '\0' || strcmp(iter.element, ".") == 0) continue;

        struct fs_directory *next_dir;
        status = fs->driver->open_directory(dir, iter.element, &next_dir);
        if (dir != inst->working_dir) {
            fs->driver->close_directory(dir);
        }
        if (!CHECK_SUCCESS(status)) return status;

        dir = next_dir;
    } while (!iter_result);

    *fs_out = fs;
    *dir_out = dir;

    return STATUS_SUCCESS;
}

static void close_ramdisk_build_entries(struct filesystem *fs, struct ramdisk_build_entry *entry)
{
    while (entry) {
        struct ramdisk_build_entry *next = entry->next;
        if (entry->dir) fs->driver->close_directory(entry->dir);
        if (entry->file) fs->driver->close(entry->file);
        free(entry);
        entry = next;
    }
}

static VlStatus append_ramdisk_file(
    struct ramdisk_builder *builder,
    struct fs_file *file,
    size_t file_size,
    uint32_t *crc32_out,
    uint32_t *file_offset_out
)
{
    VlStatus status;
    size_t offset;
    size_t read_total = 0;
    uint32_t checksum = crc32(0, NULL, 0);

    if (file_size > UINT32_MAX) return STATUS_INVALID_VALUE;

    status = ramdisk_reserve(builder, file_size, &offset);
    if (!CHECK_SUCCESS(status)) return status;
    if (offset > UINT32_MAX) return STATUS_INVALID_VALUE;

    status = file->fs->driver->seek(file, 0, SEEK_SET);
    if (!CHECK_SUCCESS(status)) return status;

    while (read_total < file_size) {
        size_t read_count = 0;
        status = file->fs->driver->read(
            file,
            builder->data + offset + read_total,
            file_size - read_total,
            &read_count
        );
        if (!CHECK_SUCCESS(status)) return status;
        if (read_count == 0) return STATUS_UNEXPECTED_RESULT;

        checksum = crc32(checksum, builder->data + offset + read_total, read_count);
        read_total += read_count;
    }

    *crc32_out = checksum;
    *file_offset_out = offset;

    return STATUS_SUCCESS;
}

static VlStatus get_file_size(struct fs_file *file, size_t fallback_size, size_t *file_size_out)
{
    VlStatus status;
    off_t current;
    off_t end;

    if (!file || !file_size_out) return STATUS_INVALID_VALUE;

    status = file->fs->driver->tell(file, &current);
    if (!CHECK_SUCCESS(status)) {
        *file_size_out = fallback_size;
        return STATUS_SUCCESS;
    }

    status = file->fs->driver->seek(file, 0, SEEK_END);
    if (!CHECK_SUCCESS(status)) {
        *file_size_out = fallback_size;
        return STATUS_SUCCESS;
    }

    status = file->fs->driver->tell(file, &end);
    if (!CHECK_SUCCESS(status)) {
        (void)file->fs->driver->seek(file, current, SEEK_SET);
        *file_size_out = fallback_size;
        return STATUS_SUCCESS;
    }

    (void)file->fs->driver->seek(file, current, SEEK_SET);
    if (end < 0) return STATUS_INVALID_VALUE;

    *file_size_out = (size_t)end;
    return STATUS_SUCCESS;
}

static VlStatus append_ramdisk_directory(
    struct ramdisk_builder *builder, struct fs_directory *dir, uint32_t *dir_offset_out
)
{
    VlStatus status;
    struct ramdisk_build_entry *head = NULL;
    struct ramdisk_build_entry **tail = &head;
    struct ramdisk_build_entry *entry;
    size_t dir_offset;
    size_t end_offset;
    struct fs_directory_entry dirent;

    status = ramdisk_reserve(builder, 0, &dir_offset);
    if (!CHECK_SUCCESS(status)) return status;
    if (dir_offset > UINT32_MAX) return STATUS_INVALID_VALUE;

    status = dir->fs->driver->rewind_directory(dir);
    if (!CHECK_SUCCESS(status)) return status;

    for (;;) {
        status = dir->fs->driver->iter_directory(dir, &dirent);
        if (!CHECK_SUCCESS(status)) {
            if (status == STATUS_END_OF_LIST) break;
            goto has_error;
        }

        if (strcmp(dirent.name, ".") == 0 || strcmp(dirent.name, "..") == 0) continue;

        size_t name_len = strlen(dirent.name);
        if (name_len > UINT8_MAX) {
            status = STATUS_INVALID_VALUE;
            goto has_error;
        }

        entry = calloc(1, sizeof(*entry));
        if (!entry) {
            status = STATUS_INSUFFICIENT_MEMORY;
            goto has_error;
        }
        memcpy(&entry->dirent, &dirent, sizeof(entry->dirent));

        status = dir->fs->driver->open_directory(dir, dirent.name, &entry->dir);
        if (CHECK_SUCCESS(status)) {
            entry->is_directory = 1;
        } else {
            size_t file_size;

            entry->dir = NULL;
            status = dir->fs->driver->open(dir, dirent.name, &entry->file);
            if (!CHECK_SUCCESS(status)) {
                free(entry);
                goto has_error;
            }
            status = get_file_size(entry->file, dirent.size, &file_size);
            if (!CHECK_SUCCESS(status)) {
                dir->fs->driver->close(entry->file);
                free(entry);
                goto has_error;
            }
            entry->dirent.size = file_size;
            LOG_DEBUG("ramdisk file: %s size=%" PRIu64 "\n", dirent.name, entry->dirent.size);
            if (entry->dirent.size > UINT32_MAX) {
                dir->fs->driver->close(entry->file);
                free(entry);
                status = STATUS_INVALID_VALUE;
                goto has_error;
            }
        }

        size_t entry_size = ALIGN(sizeof(struct StLoad_RamdiskDirEntry) + name_len, 4);
        if (entry_size > UINT16_MAX) {
            status = STATUS_INVALID_VALUE;
            if (entry->dir) dir->fs->driver->close_directory(entry->dir);
            if (entry->file) dir->fs->driver->close(entry->file);
            free(entry);
            goto has_error;
        }

        status = ramdisk_reserve(builder, entry_size, &entry->entry_offset);
        if (!CHECK_SUCCESS(status)) {
            if (entry->dir) dir->fs->driver->close_directory(entry->dir);
            if (entry->file) dir->fs->driver->close(entry->file);
            free(entry);
            goto has_error;
        }

        struct StLoad_RamdiskDirEntry *rdent = (void *)(builder->data + entry->entry_offset);
        rdent->type = entry->is_directory ? RDET_DIRECTORY : RDET_FILE;
        rdent->name_len = name_len;
        rdent->entry_size = entry_size;
        memcpy(rdent->name, dirent.name, name_len);

        printf("Added %s %s to ramdisk\n", entry->is_directory ? "directory" : "file", dirent.name);

        *tail = entry;
        tail = &entry->next;
    }

    status = ramdisk_reserve(builder, sizeof(struct StLoad_RamdiskDirEntry), &end_offset);
    if (!CHECK_SUCCESS(status)) goto has_error;

    struct StLoad_RamdiskDirEntry *end_entry = (void *)(builder->data + end_offset);
    end_entry->type = RDET_END;
    end_entry->entry_size = sizeof(*end_entry);

    for (entry = head; entry; entry = entry->next) {
        uint32_t offset;

        if (entry->is_directory) {
            status = append_ramdisk_directory(builder, entry->dir, &offset);
            if (!CHECK_SUCCESS(status)) goto has_error;

            struct StLoad_RamdiskDirEntry *rdent = (void *)(builder->data + entry->entry_offset);
            rdent->directory.file_offset = offset;
        } else {
            uint32_t checksum;
            status =
                append_ramdisk_file(builder, entry->file, entry->dirent.size, &checksum, &offset);
            if (!CHECK_SUCCESS(status)) goto has_error;

            struct StLoad_RamdiskDirEntry *rdent = (void *)(builder->data + entry->entry_offset);
            rdent->file.file_size = entry->dirent.size;
            rdent->file.file_crc32 = checksum;
            rdent->file.file_offset = offset;
        }
    }

    close_ramdisk_build_entries(dir->fs, head);
    *dir_offset_out = dir_offset;

    return STATUS_SUCCESS;

has_error:
    close_ramdisk_build_entries(dir->fs, head);
    return status;
}

VlStatus Lf_CountRamdiskFrameExtents(const struct Lf_RamdiskImage *image, uint32_t *count_out)
{
    VlStatus status;
    uint32_t count = 0;
    pfn_t prev_pfn = 0;

    if (!image || image->page_count == 0) {
        *count_out = 0;
        return STATUS_SUCCESS;
    }

    for (size_t i = 0; i < image->page_count; i++) {
        pfn_t pfn;
        status = mm_vpn_to_pfn(((uintptr_t)image->vaddr / PAGE_SIZE) + i, &pfn);
        if (!CHECK_SUCCESS(status)) return status;

        if (i == 0 || prev_pfn + 1 != pfn) {
            count++;
        }
        prev_pfn = pfn;
    }

    *count_out = count;

    return STATUS_SUCCESS;
}

VlStatus Lf_FillRamdiskFrameEntries(
    struct StLoad_BootInfoUnavailableFrameEntry *entries, const struct Lf_RamdiskImage *image
)
{
    VlStatus status;
    uint32_t filled_entries = 0;

    if (!image || image->page_count == 0) return STATUS_SUCCESS;

    for (size_t i = 0; i < image->page_count; i++) {
        pfn_t pfn;
        status = mm_vpn_to_pfn(((uintptr_t)image->vaddr / PAGE_SIZE) + i, &pfn);
        if (!CHECK_SUCCESS(status)) return status;

        if (filled_entries > 0 &&
            entries[filled_entries - 1].pfn_base + entries[filled_entries - 1].count == pfn) {
            entries[filled_entries - 1].count++;
            continue;
        }

        entries[filled_entries].pfn_base = pfn;
        entries[filled_entries].count = 1;
        entries[filled_entries].type = BEUT_RAMDISK;
        filled_entries++;
    }

    return STATUS_SUCCESS;
}

VlStatus Lf_FillRamdiskExtents(
    struct StLoad_BootInfoRamdiskExtent *extents, const struct Lf_RamdiskImage *image
)
{
    VlStatus status;
    uint32_t filled_extents = 0;
    size_t remaining_size;

    if (!image || image->page_count == 0) return STATUS_SUCCESS;

    remaining_size = image->size;
    for (size_t i = 0; i < image->page_count; i++) {
        pfn_t pfn;
        uint32_t page_size = remaining_size < PAGE_SIZE ? remaining_size : PAGE_SIZE;

        status = mm_vpn_to_pfn(((uintptr_t)image->vaddr / PAGE_SIZE) + i, &pfn);
        if (!CHECK_SUCCESS(status)) return status;

        if (filled_extents > 0 &&
            extents[filled_extents - 1].paddr + extents[filled_extents - 1].size ==
                pfn * PAGE_SIZE) {
            extents[filled_extents - 1].size += page_size;
        } else {
            extents[filled_extents].paddr = pfn * PAGE_SIZE;
            extents[filled_extents].size = page_size;
            extents[filled_extents].reserved = 0;
            filled_extents++;
        }

        remaining_size -= page_size;
    }

    return STATUS_SUCCESS;
}

VlStatus Lf_BuildRamdisk(
    struct shell_instance *inst, const char *path, struct Lf_RamdiskImage *image_out
)
{
    VlStatus status;
    struct filesystem *fs = NULL;
    struct fs_directory *root_dir = NULL;
    struct ramdisk_builder builder = {0};
    struct StLoad_RamdiskHeader *header;
    uint32_t rootdir_offset;
    size_t image_size;
    size_t page_count;
    vpn_t vpn;
    uint32_t extent_count;

    status = ramdisk_reserve(&builder, sizeof(*header), NULL);
    if (!CHECK_SUCCESS(status)) goto has_error;

    status = open_directory_path(inst, path, &fs, &root_dir);
    if (!CHECK_SUCCESS(status)) goto has_error;

    status = append_ramdisk_directory(&builder, root_dir, &rootdir_offset);
    if (!CHECK_SUCCESS(status)) goto has_error;

    header = (void *)builder.data;
    header->rootdir_offset = rootdir_offset;

    image_size = builder.size;
    if (image_size > UINT32_MAX) {
        status = STATUS_INVALID_VALUE;
        goto has_error;
    }
    page_count = ALIGN_DIV(image_size, PAGE_SIZE);
    if (page_count == 0) page_count = 1;

    status = mm_allocate_pages(page_count, &vpn);
    if (!CHECK_SUCCESS(status)) goto has_error;

    void *image_vaddr = (void *)(vpn * PAGE_SIZE);
    memset(image_vaddr, 0, page_count * PAGE_SIZE);
    memcpy(image_vaddr, builder.data, image_size);

    struct Lf_RamdiskImage image = {
        .vaddr = image_vaddr,
        .page_count = page_count,
        .size = image_size,
    };
    status = Lf_CountRamdiskFrameExtents(&image, &extent_count);
    if (!CHECK_SUCCESS(status)) goto has_error;

    if (root_dir && root_dir != inst->working_dir) fs->driver->close_directory(root_dir);
    free(builder.data);

    image_out->vaddr = image_vaddr;
    image_out->page_count = page_count;
    image_out->size = image_size;
    image_out->extent_count = extent_count;

    return STATUS_SUCCESS;

has_error:
    if (root_dir && root_dir != inst->working_dir) fs->driver->close_directory(root_dir);
    free(builder.data);
    return status;
}
