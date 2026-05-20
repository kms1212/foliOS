#ifndef LOAD_FOLIOS_H
#define LOAD_FOLIOS_H

#include <stddef.h>
#include <stdint.h>

#include <vellum/compiler.h>
#include <vellum/elf.h>
#include <vellum/shell.h>
#include <vellum/status.h>

#include <stload/bootinfo.h>

#define MODULE_NAME "load_folios"

struct Lf_RamdiskImage {
    void *vaddr;
    size_t page_count;
    size_t size;
    uint32_t extent_count;
};

VlStatus Lf_LoadKernel(
    const char *path,
    const char *argv0,
    struct elf_file **elf_out,
    void **load_paddr_out,
    size_t *program_size_out
);

VlStatus Lf_BuildRamdisk(
    struct shell_instance *inst, const char *path, struct Lf_RamdiskImage *image_out
);

VlStatus Lf_CountRamdiskFrameExtents(const struct Lf_RamdiskImage *image, uint32_t *count_out);

VlStatus Lf_FillRamdiskFrameEntries(
    struct StLoad_BootInfoUnavailableFrameEntry *entries, const struct Lf_RamdiskImage *image
);

VlStatus Lf_FillRamdiskExtents(
    struct StLoad_BootInfoRamdiskExtent *extents, const struct Lf_RamdiskImage *image
);

VlStatus Lf_MakeBootInfoTable(
    struct elf_file *elf,
    size_t program_size,
    const char *argv0,
    int kernel_argc,
    char **kernel_argv,
    void *load_paddr,
    const struct Lf_RamdiskImage *ramdisk,
    struct StLoad_BootInfoTableHeader **btblhdr_out
);

void Lf_PrepareKernelHandoff(void);
__noreturn void Lf_JumpKernel(void *entry, struct StLoad_BootInfoTableHeader *btblhdr);

#endif
