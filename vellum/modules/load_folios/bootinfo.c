#include "load_folios.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <vellum/acpi.h>
#include <vellum/arch/intrinsics/register.h>
#include <vellum/arch/mmu.h>
#include <vellum/device.h>
#include <vellum/interface/video.h>
#include <vellum/log.h>
#include <vellum/macros.h>
#include <vellum/mm.h>
#include <vellum/plat/bios/mem.h>
#include <vellum/plat/page.h>

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

static uint32_t count_pagetable_frame(void)
{
    uint32_t count = 0;

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

    if (max_count > 0) {
        entries[filled_entries].pfn_base = (VlA_ReadCr3() & 0xFFFFF000) >> 12;
        entries[filled_entries].count = 1;
        entries[filled_entries].type = BEUT_PAGETABLE;
        filled_entries++;
        max_count--;
    }

    for (int i = 0; i < 1023; i++) {
        if (!_pc_page_dir->pde[i].dir.p) continue;
        if (max_count == 0) break;

        entries[filled_entries].pfn_base = _pc_page_dir->pde[i].dir.base;
        entries[filled_entries].count = 1;
        entries[filled_entries].type = BEUT_PAGETABLE;
        filled_entries++;
        max_count--;
    }
}

static VlStatus count_mapped_frame_extents(vpn_t vpn, size_t page_count, uint32_t *count_out)
{
    VlStatus status;
    pfn_t prev_pfn = 0;
    uint32_t count = 0;

    if (!count_out) return STATUS_INVALID_VALUE;

    for (size_t i = 0; i < page_count; i++) {
        pfn_t pfn;

        status = mm_vpn_to_pfn(vpn + i, &pfn);
        if (!CHECK_SUCCESS(status)) return status;

        if (i == 0 || prev_pfn + 1 != pfn) {
            count++;
        }

        prev_pfn = pfn;
    }

    *count_out = count;

    return STATUS_SUCCESS;
}

static VlStatus fill_kernel_frame_entries(
    struct StLoad_BootInfoUnavailableFrameEntry *entries, size_t page_count, void *load_vaddr
)
{
    VlStatus status;
    pfn_t pfn;
    uint32_t filled_entries = 0;

    for (size_t i = 0; i < page_count; i++) {
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

VlStatus Lf_MakeBootInfoTable(
    struct elf_file *elf,
    size_t program_size,
    const char *argv0,
    int kernel_argc,
    char **kernel_argv,
    void *load_paddr,
    const struct Lf_RamdiskImage *ramdisk,
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
    size_t kernel_page_count;
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

    (void)elf;

    if (!ramdisk) return STATUS_INVALID_VALUE;

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
    btblhdrsize += sizeof("vellum");
    btblhdrsize += sizeof("0.0.1");
    btblhdrsize += sizeof("kms1212");
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
    kernel_page_count = ALIGN_DIV(program_size, PAGE_SIZE);
    status = count_mapped_frame_extents(
        (uintptr_t)load_paddr >> 12,
        kernel_page_count,
        &kernel_ufent_count
    );
    if (!CHECK_SUCCESS(status)) return status;
    status = Lf_CountRamdiskFrameExtents(ramdisk, &ramdisk_ufent_count);
    if (!CHECK_SUCCESS(status)) return status;

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

    btblentsize += ALIGN(sizeof(*benthdr), 16);
    btblentsize += ALIGN(
        sizeof(*entry_ramdisk) +
            (ramdisk->extent_count * sizeof(struct StLoad_BootInfoRamdiskExtent)),
        16
    );
    btblentcount++;

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
    strtab_cursor += sizeof("vellum");
    strcpy(&btblhdr->strtab[strtab_cursor], "0.0.1");
    entry_loader_info->version_offset = strtab_cursor;
    strtab_cursor += sizeof("0.0.1");
    strcpy(&btblhdr->strtab[strtab_cursor], "kms1212");
    entry_loader_info->author_offset = strtab_cursor;
    strtab_cursor += sizeof("kms1212");

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
        status = VlBiosP_QueryMemoryMap(&smap_cursor, &smap_entry, sizeof(smap_entry));
        if (!CHECK_SUCCESS(status)) return status;

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
        kernel_page_count,
        load_paddr
    );
    if (!CHECK_SUCCESS(status)) return status;
    status = Lf_FillRamdiskFrameEntries(
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

    status = Lf_FillRamdiskExtents(entry_ramdisk->extents, ramdisk);
    if (!CHECK_SUCCESS(status)) return status;

    *btblhdr_out = btblhdr;

    return STATUS_SUCCESS;
}
