#include <inttypes.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vellum/arch/intrinsics/register.h>
#include <vellum/arch/mmu.h>

#include <vellum/plat/bios/mem.h>
#include <vellum/plat/page.h>

#include <vellum/acpi.h>
#include <vellum/compiler.h>
#include <vellum/device.h>
#include <vellum/elf.h>
#include <vellum/interface/video.h>
#include <vellum/log.h>
#include <vellum/macros.h>
#include <vellum/mm.h>
#include <vellum/path.h>
#include <vellum/shell.h>
#include <vellum/status.h>

#include <loadst/bootinfo.h>

#define MODULE_NAME "loadst"

extern int __stage1_end;

__noreturn static void jump_kernel(void *entry, struct bootinfo_table_header *btblhdr)
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
    struct bootinfo_unavailable_frame_entry *entries, uint32_t max_count
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
    struct bootinfo_unavailable_frame_entry *entries, uint32_t max_count, void *load_vaddr
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

            status = VlElf_LoadProgram(elf, i, NULL);
            if (!CHECK_SUCCESS(status)) return status;
        }
    } else if (ident.class == ELFCLASS64) {
        for (int i = 0; i < elf->ehdr64.phnum; i++) {
            status = VlElf_GetProgramHeader(elf, i, &phdr64, sizeof(phdr64));
            if (!CHECK_SUCCESS(status)) return status;

            if (phdr64.type != PT_LOAD) continue;

            status = VlElf_LoadProgram(elf, i, NULL);
            if (!CHECK_SUCCESS(status)) return status;
        }
    }

    *load_paddr_out = load_paddr;
    *elf_out = elf;
    *program_size_out = program_size;

    return STATUS_SUCCESS;
}

static VlStatus make_bootinfo_table(
    struct elf_file *elf,
    size_t program_size,
    int argc,
    char **argv,
    void *load_paddr,
    struct bootinfo_table_header **btblhdr_out
)
{
    VlStatus status;
    struct device *fbdev;
    const struct video_interface *vidif;
    struct video_hw_mode_info hwmode;
    int video_mode;
    size_t btblentsize;
    size_t btblhdrsize;
    uint16_t btblentcount;
    int mmap_entry_count;
    uint32_t smap_cursor;
    uint32_t pagetable_frame_count;
    uint32_t kernel_frame_count;
    uint32_t kernel_ufent_count;
    struct smap_entry smap_entry;
    const struct acpi_rsdp *rsdp;
    struct bootinfo_table_header *btblhdr;
    struct bootinfo_entry_header *benthdr;
    struct bootinfo_entry_command_args *entry_command_args;
    struct bootinfo_entry_loader_info *entry_loader_info;
    struct bootinfo_entry_memory_map *entry_memory_map;
    struct bootinfo_entry_acpi_rsdp *entry_acpi_rsdp;
    struct bootinfo_entry_framebuffer *entry_framebuffer;
    struct bootinfo_entry_unavailable_frames *entry_unavailable_frames;
    struct bootinfo_entry_pagetable_vpn *entry_pagetable_vpn;
    uint32_t strtab_cursor;

    status = VlDev_Find("video0", &fbdev);
    if (!CHECK_SUCCESS(status)) {
        fprintf(stderr, "%s: cannot find device\n", argv[0]);
        return status;
    }

    status = fbdev->driver->get_interface(fbdev, "video", (const void **)&vidif);
    if (!CHECK_SUCCESS(status)) return status;

    status = vidif->get_mode(fbdev, &video_mode);
    if (!CHECK_SUCCESS(status)) {
        fprintf(stderr, "%s: cannot get current video mode\n", argv[0]);
        return status;
    }

    status = vidif->get_hw_mode_info(fbdev, video_mode, &hwmode);
    if (!CHECK_SUCCESS(status)) {
        fprintf(stderr, "%s: cannot get video mode hardware info\n", argv[0]);
        return status;
    }

    btblentsize = 0;
    btblhdrsize = 0;
    btblentcount = 0;

    /* add header size */
    btblhdrsize += sizeof(*btblhdr);

    /* add command args entry size */
    for (int i = 2; i < argc; i++) {
        btblhdrsize += strlen(argv[i]) + 1;
    }
    btblentsize += ALIGN(sizeof(*benthdr), 16);
    btblentsize += ALIGN(
        sizeof(*entry_command_args) + ((argc - 2) * sizeof(*entry_command_args->arg_offsets)),
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
    btblentsize += ALIGN(sizeof(*benthdr), 16);
    btblentsize += ALIGN(
        sizeof(*entry_unavailable_frames) +
            ((pagetable_frame_count + kernel_ufent_count) *
                sizeof(*entry_unavailable_frames->entries)),
        16
    );
    btblentcount++;

    /* add pagetable vpn entry size */
    btblentsize += ALIGN(sizeof(*benthdr), 16);
    btblentsize += ALIGN(sizeof(*entry_pagetable_vpn), 16);
    btblentcount++;

    /* align header size */
    btblhdrsize = ALIGN(btblhdrsize, 16);

    /* allocate table */
    btblhdr = (void *)ALIGN((uintptr_t)&__stage1_end, 16);

    /* fill header */
    btblhdr->flags = 0;
    btblhdr->version = BTV_CURRENT;
    btblhdr->header_size = ALIGN(btblhdrsize, 16);
    btblhdr->entry_count = btblentcount;
    btblhdr->size = btblhdrsize + btblentsize;
    strtab_cursor = 0;

    /* fill command args entry */
    benthdr = (void *)((uintptr_t)btblhdr + btblhdr->header_size);
    benthdr->type = BET_COMMAND_ARGS;
    benthdr->header_size = ALIGN(sizeof(*benthdr), 16);
    benthdr->size = benthdr->header_size +
        ALIGN(sizeof(*entry_command_args) + ((argc - 2) * sizeof(*entry_command_args->arg_offsets)),
              16);
    benthdr->flags = BEF_REQUIRED;

    entry_command_args = (void *)((uintptr_t)benthdr + benthdr->header_size);
    entry_command_args->arg_count = argc - 2;
    for (int i = 2; i < argc; i++) {
        strcpy(&btblhdr->strtab[strtab_cursor], argv[i]);
        entry_command_args->arg_offsets[i - 2] = strtab_cursor;
        strtab_cursor += strlen(argv[i]) + 1;
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
    strncpy(entry_acpi_rsdp->oemid, rsdp->oemid, sizeof(entry_acpi_rsdp->oemid));
    entry_acpi_rsdp->revision = rsdp->revision;
    entry_acpi_rsdp->size = rsdp->revision >= 2 ? rsdp->length : 20;
    entry_acpi_rsdp->rsdt_addr = rsdp->rsdt_addr;
    if (rsdp->revision >= 2) {
        entry_acpi_rsdp->xsdt_addr = rsdp->xsdt_addr;
    } else {
        entry_acpi_rsdp->xsdt_addr = 0;
    }

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
                  ((pagetable_frame_count + kernel_ufent_count) *
                      sizeof(*entry_unavailable_frames->entries)),
              16);
    benthdr->flags = BEF_REQUIRED;

    entry_unavailable_frames = (void *)((uintptr_t)benthdr + benthdr->header_size);
    entry_unavailable_frames->entry_count = pagetable_frame_count + kernel_ufent_count;
    LOG_DEBUG("entry count: %" PRIu32 "\n", entry_unavailable_frames->entry_count);
    fill_pagetable_frame_entries(entry_unavailable_frames->entries, pagetable_frame_count);
    status = fill_kernel_frame_entries(
        &entry_unavailable_frames->entries[pagetable_frame_count],
        kernel_frame_count,
        load_paddr
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

    *btblhdr_out = btblhdr;

    return STATUS_SUCCESS;
}

static int loadst_handler(struct shell_instance *inst, int argc, char **argv)
{
    VlStatus status;
    struct elf_file *elf = NULL;
    void *load_paddr;
    size_t program_size;
    struct bootinfo_table_header *btblhdr;

    if (argc < 2) {
        fprintf(stderr, "usage: %s path\n", argv[0]);
        return 1;
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
    if (!CHECK_SUCCESS(status)) return 1;

    status = make_bootinfo_table(elf, program_size, argc, argv, load_paddr, &btblhdr);
    if (!CHECK_SUCCESS(status)) return 1;

    // cleanup();

    /* jump to kernel */
    jump_kernel(
        (void *)(uintptr_t)(elf->ident.class == ELFCLASS32 ? elf->ehdr32.entry : elf->ehdr64.entry),
        btblhdr
    );
}

static struct command loadst_command = {
    .name = "loadst",
    .handler = loadst_handler,
    .help_message = "Load Strata kernel",
};

__constructor static void init()
{
    VlShell_RegisterCommand(&loadst_command);
}

VlStatus _start(int argc, char **argv)
{
    return STATUS_SUCCESS;
}

__destructor static void deinit(void)
{
    VlShell_UnregisterCommand(&loadst_command);
}
