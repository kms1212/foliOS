#include <inttypes.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <zlib.h>

#include <vellum/arch/intrinsics/register.h>
#include <vellum/arch/mmu.h>

#include <vellum/plat/bios/mem.h>
#include <vellum/plat/page.h>

#include <vellum/acpi.h>
#include <vellum/compiler.h>
#include <vellum/device.h>
#include <vellum/elf.h>
#include <vellum/filesystem.h>
#include <vellum/interface/video.h>
#include <vellum/log.h>
#include <vellum/macros.h>
#include <vellum/mm.h>
#include <vellum/path.h>
#include <vellum/shell.h>
#include <vellum/status.h>

#include <stload/bootinfo.h>
#include <stload/ramdisk.h>

#define MODULE_NAME "load_folios"

struct ramdisk_image {
    void *vaddr;
    size_t page_count;
    size_t size;
    uint32_t extent_count;
};

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

__noreturn static void jump_kernel(void *entry, struct StLoad_BootInfoTableHeader *btblhdr)
{
    __asm__ volatile("jmp *%1" : : "d"(btblhdr), "r"(entry));

    for (;;) {
    }
}

static VlStatus count_smap_entry(int *result)
{
    VlStatus status;
    struct smap_entry smap_entry;
    uint32_t cursor = 0;
    int count = 0;

    do {
        status = VlBiosP_QueryMemoryMap(&cursor, &smap_entry, sizeof(smap_entry));
        if (!CHECK_SUCCESS(status)) return status;

        count++;
    } while (cursor);

    if (result) *result = count;

    return STATUS_SUCCESS;
}

static int count_pagetable_frame(void)
{
    int count = 0;

    for (int i = 0; i < 1023; i++) {
        if (!_pc_page_dir->pde[i].dir.p) continue;
        count++;
    }

    return 1 + count; /* PD count + PT count */
}

static void fill_pagetable_frame_entries(
    struct StLoad_BootInfoUnavailableFrameEntry *entries, uint32_t max_count
)
{
    uint32_t filled_entries = 0;

    if (max_count-- > 0) {
        entries[filled_entries].pfn_base = (VlA_ReadCr3() & 0xFFFFF000) >> 12;
        entries[filled_entries].count = 1;
        entries[filled_entries].type = BEUT_PAGETABLE;
        filled_entries++;
    }

    for (int i = 0; i < 1023; i++) {
        if (!_pc_page_dir->pde[i].dir.p) continue;
        if (max_count-- <= 0) break;

        entries[filled_entries].pfn_base = _pc_page_dir->pde[i].dir.base;
        entries[filled_entries].count = 1;
        entries[filled_entries].type = BEUT_PAGETABLE;
        filled_entries++;
    }
}

static int count_kernel_ufent(void *load_vaddr, uint32_t max_count)
{
    VlStatus status;
    pfn_t prev_pfn = 0, pfn;
    uint32_t count = 0;

    for (uint32_t i = 0; i < max_count; i++) {
        status = mm_vpn_to_pfn(((uintptr_t)load_vaddr >> 12) + i, &pfn);
        if (!CHECK_SUCCESS(status)) continue;

        if (prev_pfn + 1 != pfn) {
            count++;
        }

        prev_pfn = pfn;
    }

    LOG_DEBUG("count = %" PRIu32 "\n", count);

    return count;
}

static VlStatus fill_kernel_frame_entries(
    struct StLoad_BootInfoUnavailableFrameEntry *entries, uint32_t max_count, void *load_vaddr
)
{
    VlStatus status;
    pfn_t pfn;
    uint32_t filled_entries = 0;

    for (uint32_t i = 0; i < max_count; i++) {
        status = mm_vpn_to_pfn(((uintptr_t)load_vaddr >> 12) + i, &pfn);
        if (!CHECK_SUCCESS(status)) return status;

        if (filled_entries > 0 &&
            entries[filled_entries - 1].pfn_base + entries[filled_entries - 1].count == pfn) {
            entries[filled_entries - 1].count++;
            continue;
        }

        entries[filled_entries].pfn_base = pfn;
        entries[filled_entries].count = 1;
        entries[filled_entries].type = BEUT_KERNEL;
        filled_entries++;
    }

    LOG_DEBUG("filled_entries = %" PRIu32 "\n", filled_entries);

    return STATUS_SUCCESS;
}

static VlStatus load_kernel(
    const char *path,
    const char *argv0,
    struct elf_file **elf_out,
    void **load_paddr_out,
    size_t *program_size_out
)
{
    VlStatus status;
    struct elf_file *elf = NULL;
    struct elf_ident ident;
    struct elf32_phdr phdr32;
    struct elf64_phdr phdr64;
    size_t program_size = 0;
    void *load_paddr = NULL;

    status = VlElf_Open(path, &elf);
    if (!CHECK_SUCCESS(status)) {
        fprintf(stderr, "%s: failed to open file\n", argv0);
        return status;
    }

    status = VlElf_GetHeader(elf, &ident, sizeof(ident));
    if (!CHECK_SUCCESS(status)) return status;

    if (ident.class == ELFCLASS32) {
        if (elf->ehdr32.type != ET_EXEC) return STATUS_INVALID_VALUE;

        LOG_DEBUG("calculating program offset and size...\n");
        for (int i = 0; i < elf->ehdr32.phnum; i++) {
            status = VlElf_GetProgramHeader(elf, i, &phdr32, sizeof(phdr32));
            if (!CHECK_SUCCESS(status)) return status;

            if (phdr32.type != PT_LOAD) continue;

            if (!load_paddr || (uintptr_t)load_paddr > phdr32.paddr) {
                load_paddr = (void *)phdr32.paddr;
            }

            if ((uintptr_t)load_paddr + program_size < phdr32.paddr + phdr32.memsz) {
                program_size = phdr32.paddr + phdr32.memsz - (uintptr_t)load_paddr;
            }
        }
    } else if (ident.class == ELFCLASS64) {
        if (elf->ehdr64.type != ET_EXEC) return STATUS_INVALID_VALUE;

        LOG_DEBUG("calculating program offset and size...\n");
        for (int i = 0; i < elf->ehdr64.phnum; i++) {
            status = VlElf_GetProgramHeader(elf, i, &phdr64, sizeof(phdr64));
            if (!CHECK_SUCCESS(status)) return status;

            if (phdr64.type != PT_LOAD) continue;

            uintptr_t phdr_paddr = (uintptr_t)phdr64.paddr;
            if (!load_paddr || (uintptr_t)load_paddr > phdr_paddr) {
                load_paddr = (void *)phdr_paddr;
            }

            if ((uintptr_t)load_paddr + program_size < phdr_paddr + phdr64.memsz) {
                program_size = phdr_paddr + phdr64.memsz - (uintptr_t)load_paddr;
            }
        }
    } else {
        fprintf(stderr, "%s: unsupported elf class\n", argv0);
        return STATUS_INVALID_VALUE;
    }
    if (!load_paddr) return STATUS_INVALID_VALUE;

    LOG_DEBUG("offset=0x%p, size=%08zX\n", load_paddr, program_size);

    status =
        mm_allocate_pages_to((uintptr_t)load_paddr / PAGE_SIZE, ALIGN_DIV(program_size, PAGE_SIZE));
    if (!CHECK_SUCCESS(status)) return status;

    LOG_DEBUG("loading program...\n");
    if (ident.class == ELFCLASS32) {
        for (int i = 0; i < elf->ehdr32.phnum; i++) {
            status = VlElf_GetProgramHeader(elf, i, &phdr32, sizeof(phdr32));
            if (!CHECK_SUCCESS(status)) return status;

            if (phdr32.type != PT_LOAD) continue;

            printf(
                "PHDR #%d:\n"
                "  paddr=0x%08" PRIX32 "\n"
                "  vaddr=0x%08" PRIX32 "\n"
                "  filesz=%08" PRIX32 "\n"
                "  memsz=%08" PRIX32 "\n",
                i,
                phdr32.paddr,
                phdr32.vaddr,
                phdr32.filesz,
                phdr32.memsz
            );

            status = VlElf_LoadProgram(elf, i, NULL);
            if (!CHECK_SUCCESS(status)) return status;
        }
    } else if (ident.class == ELFCLASS64) {
        for (int i = 0; i < elf->ehdr64.phnum; i++) {
            status = VlElf_GetProgramHeader(elf, i, &phdr64, sizeof(phdr64));
            if (!CHECK_SUCCESS(status)) return status;

            if (phdr64.type != PT_LOAD) continue;

            printf(
                "PHDR #%d:\n"
                "  paddr=0x%016" PRIX64 "\n"
                "  vaddr=0x%016" PRIX64 "\n"
                "  filesz=%016" PRIX64 "\n"
                "  memsz=%016" PRIX64 "\n",
                i,
                phdr64.paddr,
                phdr64.vaddr,
                phdr64.filesz,
                phdr64.memsz
            );

            status = VlElf_LoadProgram(elf, i, NULL);
            if (!CHECK_SUCCESS(status)) return status;
        }
    }

    *load_paddr_out = load_paddr;
    *elf_out = elf;
    *program_size_out = program_size;

    return STATUS_SUCCESS;
}

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

static VlStatus count_ramdisk_frame_extents(const struct ramdisk_image *image, uint32_t *count_out)
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

static VlStatus fill_ramdisk_frame_entries(
    struct StLoad_BootInfoUnavailableFrameEntry *entries, const struct ramdisk_image *image
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

static VlStatus fill_bootinfo_frame_entries(
    struct StLoad_BootInfoUnavailableFrameEntry *entries, vpn_t btbl_vpn, size_t page_count
)
{
    VlStatus status;

    for (size_t i = 0; i < page_count; i++) {
        pfn_t pfn;

        status = mm_vpn_to_pfn(btbl_vpn + i, &pfn);
        if (!CHECK_SUCCESS(status)) return status;

        entries[i].pfn_base = pfn;
        entries[i].count = 1;
        entries[i].type = BEUT_BOOTINFO;
    }

    return STATUS_SUCCESS;
}

static VlStatus fill_ramdisk_extents(
    struct StLoad_BootInfoRamdiskExtent *extents, const struct ramdisk_image *image
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

static VlStatus build_ramdisk(
    struct shell_instance *inst, const char *path, struct ramdisk_image *image_out
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

    struct ramdisk_image image = {
        .vaddr = image_vaddr,
        .page_count = page_count,
        .size = image_size,
    };
    status = count_ramdisk_frame_extents(&image, &extent_count);
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

static VlStatus make_bootinfo_table(
    struct elf_file *elf,
    size_t program_size,
    const char *argv0,
    int kernel_argc,
    char **kernel_argv,
    void *load_paddr,
    const struct ramdisk_image *ramdisk,
    struct StLoad_BootInfoTableHeader **btblhdr_out
)
{
    VlStatus status;
    struct device *fbdev;
    const struct video_interface *vidif;
    struct video_hw_mode_info hwmode;
    int video_mode;
    size_t btblentsize;
    size_t btblhdrsize;
    size_t btblsize;
    size_t btbl_page_count;
    uint16_t btblentcount;
    int mmap_entry_count;
    uint32_t smap_cursor;
    uint32_t pagetable_frame_count;
    uint32_t kernel_frame_count;
    uint32_t kernel_ufent_count;
    uint32_t ramdisk_ufent_count = 0;
    uint32_t bootinfo_ufent_count = 0;
    uint32_t base_ufent_count;
    struct smap_entry smap_entry;
    const struct acpi_rsdp *rsdp;
    struct StLoad_BootInfoTableHeader *btblhdr;
    struct StLoad_BootInfoEntryHeader *benthdr;
    struct StLoad_BootInfoEntryCommandArgs *entry_command_args;
    struct StLoad_BootInfoEntryLoaderInfo *entry_loader_info;
    struct StLoad_BootInfoEntryMemoryMap *entry_memory_map;
    struct StLoad_BootInfoEntryAcpiRsdp *entry_acpi_rsdp;
    struct StLoad_BootInfoEntryFramebuffer *entry_framebuffer;
    struct StLoad_BootInfoEntryUnavailableFrames *entry_unavailable_frames;
    struct StLoad_BootInfoEntryPagetableVpn *entry_pagetable_vpn;
    struct StLoad_BootInfoEntryRamdisk *entry_ramdisk;
    uint32_t strtab_cursor;
    size_t unavailable_payload_base_size;
    size_t unavailable_entry_size;
    pfn_t btbl_pfn;
    vpn_t btbl_vpn;

    status = VlDev_Find("video0", &fbdev);
    if (!CHECK_SUCCESS(status)) {
        fprintf(stderr, "%s: cannot find device\n", argv0);
        return status;
    }

    status = fbdev->driver->get_interface(fbdev, "video", (const void **)&vidif);
    if (!CHECK_SUCCESS(status)) return status;

    status = vidif->get_mode(fbdev, &video_mode);
    if (!CHECK_SUCCESS(status)) {
        fprintf(stderr, "%s: cannot get current video mode\n", argv0);
        return status;
    }

    status = vidif->get_hw_mode_info(fbdev, video_mode, &hwmode);
    if (!CHECK_SUCCESS(status)) {
        fprintf(stderr, "%s: cannot get video mode hardware info\n", argv0);
        return status;
    }

    btblentsize = 0;
    btblhdrsize = 0;
    btblentcount = 0;

    /* add header size */
    btblhdrsize += sizeof(*btblhdr);

    /* add command args entry size */
    for (int i = 0; i < kernel_argc; i++) {
        btblhdrsize += strlen(kernel_argv[i]) + 1;
    }
    btblentsize += ALIGN(sizeof(*benthdr), 16);
    btblentsize += ALIGN(
        sizeof(*entry_command_args) + (kernel_argc * sizeof(*entry_command_args->arg_offsets)),
        16
    );
    btblentcount++;

    /* add loader info entry size */
    btblhdrsize += sizeof("vellum") + 1;
    btblhdrsize += sizeof("0.0.1") + 1;
    btblhdrsize += sizeof("kms1212") + 1;
    btblentsize += ALIGN(sizeof(*benthdr), 16);
    btblentsize += ALIGN(sizeof(*entry_loader_info), 16);
    btblentcount++;

    /* add memory map entry size */
    status = count_smap_entry(&mmap_entry_count);
    if (!CHECK_SUCCESS(status)) return status;

    btblentsize += ALIGN(sizeof(*benthdr), 16);
    btblentsize += ALIGN(
        sizeof(*entry_memory_map) + (mmap_entry_count * sizeof(*entry_memory_map->entries)),
        16
    );
    btblentcount++;

    /* add system disk entry size */
    // TODO: implement

    /* add acpi rsdp entry size */
    btblentsize += ALIGN(sizeof(*benthdr), 16);
    btblentsize += ALIGN(sizeof(*entry_acpi_rsdp), 16);
    btblentcount++;

    /* add framebuffer entry size */
    btblentsize += ALIGN(sizeof(*benthdr), 16);
    btblentsize += ALIGN(sizeof(*entry_framebuffer), 16);
    btblentcount++;

    /* add unavailable frames entry size */
    pagetable_frame_count = count_pagetable_frame();
    kernel_frame_count = ALIGN_DIV(program_size, PAGE_SIZE);
    kernel_ufent_count = count_kernel_ufent(load_paddr, kernel_frame_count);
    if (ramdisk) {
        status = count_ramdisk_frame_extents(ramdisk, &ramdisk_ufent_count);
        if (!CHECK_SUCCESS(status)) return status;
    }
    base_ufent_count = pagetable_frame_count + kernel_ufent_count + ramdisk_ufent_count;
    unavailable_payload_base_size = sizeof(*entry_unavailable_frames) +
        (base_ufent_count * sizeof(*entry_unavailable_frames->entries));
    unavailable_entry_size = ALIGN(unavailable_payload_base_size, 16);
    btblentsize += ALIGN(sizeof(*benthdr), 16);
    btblentsize += unavailable_entry_size;
    btblentcount++;

    /* add pagetable vpn entry size */
    btblentsize += ALIGN(sizeof(*benthdr), 16);
    btblentsize += ALIGN(sizeof(*entry_pagetable_vpn), 16);
    btblentcount++;

    if (ramdisk) {
        btblentsize += ALIGN(sizeof(*benthdr), 16);
        btblentsize += ALIGN(
            sizeof(*entry_ramdisk) +
                (ramdisk->extent_count * sizeof(struct StLoad_BootInfoRamdiskExtent)),
            16
        );
        btblentcount++;
    }

    /* align header size */
    btblhdrsize = ALIGN(btblhdrsize, 16);

    /* account for the table's own backing frames before allocating it */
    btblsize = btblhdrsize + btblentsize;
    for (;;) {
        size_t new_unavailable_entry_size;
        uint32_t needed_bootinfo_ufent_count;

        btbl_page_count = ALIGN_DIV(btblsize, PAGE_SIZE);
        if (btbl_page_count == 0) btbl_page_count = 1;
        needed_bootinfo_ufent_count = btbl_page_count;

        if (needed_bootinfo_ufent_count == bootinfo_ufent_count) break;

        new_unavailable_entry_size = ALIGN(
            unavailable_payload_base_size +
                (needed_bootinfo_ufent_count * sizeof(*entry_unavailable_frames->entries)),
            16
        );
        btblentsize += new_unavailable_entry_size - unavailable_entry_size;
        unavailable_entry_size = new_unavailable_entry_size;
        bootinfo_ufent_count = needed_bootinfo_ufent_count;
        btblsize = btblhdrsize + btblentsize;
    }

    /* allocate table */
    status = mm_pma_allocate_frame(btbl_page_count, &btbl_pfn);
    if (!CHECK_SUCCESS(status)) return status;

    btbl_vpn = btbl_pfn;
    status = mm_map(btbl_pfn, btbl_vpn, btbl_page_count, PF_DEFAULT);
    if (!CHECK_SUCCESS(status)) return status;

    btblhdr = (void *)(btbl_vpn * PAGE_SIZE);
    memset(btblhdr, 0, btbl_page_count * PAGE_SIZE);

    /* fill header */
    btblhdr->flags = 0;
    btblhdr->version = BTV_CURRENT;
    btblhdr->header_size = ALIGN(btblhdrsize, 16);
    btblhdr->entry_count = btblentcount;
    btblhdr->size = btblsize;
    strtab_cursor = 0;

    /* fill command args entry */
    benthdr = (void *)((uintptr_t)btblhdr + btblhdr->header_size);
    benthdr->type = BET_COMMAND_ARGS;
    benthdr->header_size = ALIGN(sizeof(*benthdr), 16);
    benthdr->size = benthdr->header_size +
        ALIGN(sizeof(*entry_command_args) +
                  (kernel_argc * sizeof(*entry_command_args->arg_offsets)),
              16);
    benthdr->flags = BEF_REQUIRED;

    entry_command_args = (void *)((uintptr_t)benthdr + benthdr->header_size);
    entry_command_args->arg_count = kernel_argc;
    for (int i = 0; i < kernel_argc; i++) {
        strcpy(&btblhdr->strtab[strtab_cursor], kernel_argv[i]);
        entry_command_args->arg_offsets[i] = strtab_cursor;
        strtab_cursor += strlen(kernel_argv[i]) + 1;
    }

    /* fill loader info entry */
    benthdr = (void *)((uintptr_t)benthdr + benthdr->size);
    benthdr->type = BET_LOADER_INFO;
    benthdr->header_size = ALIGN(sizeof(*benthdr), 16);
    benthdr->size = benthdr->header_size + ALIGN(sizeof(*entry_loader_info), 16);
    benthdr->flags = BEF_REQUIRED;

    entry_loader_info = (void *)((uintptr_t)benthdr + benthdr->header_size);
    entry_loader_info->additional_entry_count = 0;
    strcpy(&btblhdr->strtab[strtab_cursor], "vellum");
    entry_loader_info->name_offset = strtab_cursor;
    strtab_cursor += sizeof("vellum") + 1;
    strcpy(&btblhdr->strtab[strtab_cursor], "0.0.1");
    entry_loader_info->version_offset = strtab_cursor;
    strtab_cursor += sizeof("0.0.1") + 1;
    strcpy(&btblhdr->strtab[strtab_cursor], "kms1212");
    entry_loader_info->author_offset = strtab_cursor;
    strtab_cursor += sizeof("kms1212") + 1;

    /* fill memory map entry */
    benthdr = (void *)((uintptr_t)benthdr + benthdr->size);
    benthdr->type = BET_MEMORY_MAP;
    benthdr->header_size = ALIGN(sizeof(*benthdr), 16);
    benthdr->size = benthdr->header_size +
        ALIGN(sizeof(*entry_memory_map) + (mmap_entry_count * sizeof(*entry_memory_map->entries)),
              16);
    benthdr->flags = BEF_REQUIRED;

    entry_memory_map = (void *)((uintptr_t)benthdr + benthdr->header_size);
    entry_memory_map->entry_count = mmap_entry_count;
    smap_cursor = 0;
    for (int i = 0; i < mmap_entry_count; i++) {
        VlBiosP_QueryMemoryMap(&smap_cursor, &smap_entry, sizeof(smap_entry));
        entry_memory_map->entries[i].base =
            (uint64_t)smap_entry.base_addr_high << 32 | smap_entry.base_addr_low;
        entry_memory_map->entries[i].size =
            (uint64_t)smap_entry.length_high << 32 | smap_entry.length_low;
        entry_memory_map->entries[i].type = smap_entry.type;
    }

    /* fill acpi rsdp entry */
    status = VlAcpi_FindRsdp(&rsdp);
    if (!CHECK_SUCCESS(status)) return status;

    benthdr = (void *)((uintptr_t)benthdr + benthdr->size);
    benthdr->type = BET_ACPI_RSDP;
    benthdr->header_size = ALIGN(sizeof(*benthdr), 16);
    benthdr->size = benthdr->header_size + ALIGN(sizeof(*entry_acpi_rsdp), 16);
    benthdr->flags = BEF_REQUIRED;

    entry_acpi_rsdp = (void *)((uintptr_t)benthdr + benthdr->header_size);
    entry_acpi_rsdp->rsdp_addr = (uintptr_t)rsdp;

    /* fill framebuffer entry */
    benthdr = (void *)((uintptr_t)benthdr + benthdr->size);
    benthdr->type = BET_FRAMEBUFFER;
    benthdr->header_size = ALIGN(sizeof(*benthdr), 16);
    benthdr->size = benthdr->header_size + ALIGN(sizeof(*entry_framebuffer), 16);
    benthdr->flags = BEF_REQUIRED;

    entry_framebuffer = (void *)((uintptr_t)benthdr + benthdr->header_size);
    entry_framebuffer->framebuffer_addr = (uintptr_t)hwmode.framebuffer;
    entry_framebuffer->width = hwmode.width;
    entry_framebuffer->pitch = hwmode.pitch;
    entry_framebuffer->height = hwmode.height;
    entry_framebuffer->bpp = hwmode.bpp;
    if (hwmode.memory_model == VMM_DIRECT) {
        entry_framebuffer->type = BEFT_DIRECT;

        entry_framebuffer->direct.red_pos = hwmode.rpos;
        entry_framebuffer->direct.red_size = hwmode.rmask;
        entry_framebuffer->direct.green_pos = hwmode.gpos;
        entry_framebuffer->direct.green_size = hwmode.gmask;
        entry_framebuffer->direct.blue_pos = hwmode.bpos;
        entry_framebuffer->direct.blue_size = hwmode.bmask;
    } else {
        entry_framebuffer->type = BEFT_TEXT;
    }

    /* fill unavailable frames entry */
    benthdr = (void *)((uintptr_t)benthdr + benthdr->size);
    benthdr->type = BET_UNAVAILABLE_FRAMES;
    benthdr->header_size = ALIGN(sizeof(*benthdr), 16);
    benthdr->size = benthdr->header_size +
        ALIGN(sizeof(*entry_unavailable_frames) +
                  ((base_ufent_count + bootinfo_ufent_count) *
                   sizeof(*entry_unavailable_frames->entries)),
              16);
    benthdr->flags = BEF_REQUIRED;

    entry_unavailable_frames = (void *)((uintptr_t)benthdr + benthdr->header_size);
    entry_unavailable_frames->entry_count = base_ufent_count + bootinfo_ufent_count;
    LOG_DEBUG("entry count: %" PRIu32 "\n", entry_unavailable_frames->entry_count);
    fill_pagetable_frame_entries(entry_unavailable_frames->entries, pagetable_frame_count);
    status = fill_kernel_frame_entries(
        &entry_unavailable_frames->entries[pagetable_frame_count],
        kernel_frame_count,
        load_paddr
    );
    if (!CHECK_SUCCESS(status)) return status;
    status = fill_ramdisk_frame_entries(
        &entry_unavailable_frames->entries[pagetable_frame_count + kernel_ufent_count],
        ramdisk
    );
    if (!CHECK_SUCCESS(status)) return status;
    status = fill_bootinfo_frame_entries(
        &entry_unavailable_frames
             ->entries[pagetable_frame_count + kernel_ufent_count + ramdisk_ufent_count],
        btbl_vpn,
        btbl_page_count
    );
    if (!CHECK_SUCCESS(status)) return status;

    /* fill pagetable vpn entry */
    benthdr = (void *)((uintptr_t)benthdr + benthdr->size);
    benthdr->type = BET_PAGETABLE_VPN;
    benthdr->header_size = ALIGN(sizeof(*benthdr), 16);
    benthdr->size = benthdr->header_size + ALIGN(sizeof(*entry_pagetable_vpn), 16);
    benthdr->flags = BEF_REQUIRED;

    entry_pagetable_vpn = (void *)((uintptr_t)benthdr + benthdr->header_size);
    entry_pagetable_vpn->vpn = (uintptr_t)_pc_page_dir / PAGE_SIZE;

    if (ramdisk) {
        benthdr = (void *)((uintptr_t)benthdr + benthdr->size);
        benthdr->type = BET_RAMDISK;
        benthdr->header_size = ALIGN(sizeof(*benthdr), 16);
        benthdr->size = benthdr->header_size +
            ALIGN(sizeof(*entry_ramdisk) +
                      (ramdisk->extent_count * sizeof(struct StLoad_BootInfoRamdiskExtent)),
                  16);
        benthdr->flags = BEF_REQUIRED;

        entry_ramdisk = (void *)((uintptr_t)benthdr + benthdr->header_size);
        entry_ramdisk->version = 0;
        entry_ramdisk->reserved[0] = 0;
        entry_ramdisk->reserved[1] = 0;
        entry_ramdisk->reserved[2] = 0;
        entry_ramdisk->size = ramdisk->size;
        entry_ramdisk->extent_count = ramdisk->extent_count;
        entry_ramdisk->reserved2 = 0;

        status = fill_ramdisk_extents(entry_ramdisk->extents, ramdisk);
        if (!CHECK_SUCCESS(status)) return status;
    }

    *btblhdr_out = btblhdr;

    return STATUS_SUCCESS;
}

static int load_folios_handler(struct shell_instance *inst, int argc, char **argv)
{
    VlStatus status;
    struct elf_file *elf = NULL;
    void *load_paddr;
    size_t program_size;
    struct StLoad_BootInfoTableHeader *btblhdr;
    struct ramdisk_image ramdisk = {0};
    const char *ramdisk_path = NULL;
    char **kernel_argv = NULL;
    int kernel_argc = 0;

    if (argc < 2) {
        fprintf(stderr, "usage: %s kernel-path [-ramdisk path] [kernel args...]\n", argv[0]);
        return 1;
    }

    kernel_argv = malloc(sizeof(*kernel_argv) * argc);
    if (!kernel_argv) return 1;

    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "-ramdisk") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "%s: -ramdisk requires a path\n", argv[0]);
                free(kernel_argv);
                return 1;
            }
            ramdisk_path = argv[++i];
            continue;
        }

        if (strncmp(argv[i], "-ramdisk=", sizeof("-ramdisk=") - 1) == 0) {
            ramdisk_path = argv[i] + sizeof("-ramdisk=") - 1;
            continue;
        }

        kernel_argv[kernel_argc++] = argv[i];
    }

    char path[PATH_MAX];
    if (VlPath_IsAbsolute(argv[1])) {
        strncpy(path, argv[1], sizeof(path) - 1);
    } else {
        strncpy(path, inst->working_dir_path, sizeof(path) - 1);
        VlPath_Join(path, sizeof(path), argv[1]);

        if (!inst->fs) {
            fprintf(stderr, "%s: filesystem not selected\n", argv[0]);
            return 1;
        }
    }

    printf("Loading kernel...\n");

    status = load_kernel(path, argv[0], &elf, &load_paddr, &program_size);
    if (!CHECK_SUCCESS(status)) {
        free(kernel_argv);
        return 1;
    }

    if (ramdisk_path) {
        printf("Building ramdisk...\n");
        status = build_ramdisk(inst, ramdisk_path, &ramdisk);
        if (!CHECK_SUCCESS(status)) {
            fprintf(stderr, "%s: failed to build ramdisk\n", argv[0]);
            free(kernel_argv);
            return 1;
        }
    }

    status = make_bootinfo_table(
        elf,
        program_size,
        argv[0],
        kernel_argc,
        kernel_argv,
        load_paddr,
        ramdisk_path ? &ramdisk : NULL,
        &btblhdr
    );
    if (!CHECK_SUCCESS(status)) {
        free(kernel_argv);
        return 1;
    }

    // cleanup();
    free(kernel_argv);

    /* jump to kernel */
    jump_kernel(
        (void *)(uintptr_t)(elf->ident.class == ELFCLASS32 ? elf->ehdr32.entry : elf->ehdr64.entry),
        btblhdr
    );
}

static struct command load_folios_command = {
    .name = "load_folios",
    .handler = load_folios_handler,
    .help_message = "Load Strata kernel",
};

__constructor static void init()
{
    VlShell_RegisterCommand(&load_folios_command);
}

VlStatus _start(int argc, char **argv)
{
    return STATUS_SUCCESS;
}

__destructor static void deinit(void)
{
    VlShell_UnregisterCommand(&load_folios_command);
}
