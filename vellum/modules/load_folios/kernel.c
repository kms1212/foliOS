#include "load_folios.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

#include <vellum/elf.h>
#include <vellum/log.h>
#include <vellum/macros.h>
#include <vellum/mm.h>
#include <vellum/plat/page.h>

static VlStatus update_program_load_range(
    uint64_t paddr, uint64_t memsz, uintptr_t *load_base, uintptr_t *load_end
)
{
    uint64_t segment_limit;
    uintptr_t segment_base;
    uintptr_t segment_end;

    if (memsz == 0) return STATUS_SUCCESS;
    if (paddr > UINTPTR_MAX || memsz > UINTPTR_MAX - paddr) return STATUS_INVALID_VALUE;

    segment_limit = paddr + memsz;
    if (segment_limit > UINTPTR_MAX - (PAGE_SIZE - 1)) return STATUS_INVALID_VALUE;

    segment_base = (uintptr_t)paddr & ~(uintptr_t)(PAGE_SIZE - 1);
    segment_end = ALIGN((uintptr_t)segment_limit, PAGE_SIZE);

    if (*load_base > segment_base) *load_base = segment_base;
    if (*load_end < segment_end) *load_end = segment_end;

    return STATUS_SUCCESS;
}

VlStatus Lf_LoadKernel(
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
    uintptr_t load_base = UINTPTR_MAX;
    uintptr_t load_end = 0;
    size_t program_size;

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

            status = update_program_load_range(phdr32.paddr, phdr32.memsz, &load_base, &load_end);
            if (!CHECK_SUCCESS(status)) return status;
        }
    } else if (ident.class == ELFCLASS64) {
        if (elf->ehdr64.type != ET_EXEC) return STATUS_INVALID_VALUE;

        LOG_DEBUG("calculating program offset and size...\n");
        for (int i = 0; i < elf->ehdr64.phnum; i++) {
            status = VlElf_GetProgramHeader(elf, i, &phdr64, sizeof(phdr64));
            if (!CHECK_SUCCESS(status)) return status;

            if (phdr64.type != PT_LOAD) continue;

            status = update_program_load_range(phdr64.paddr, phdr64.memsz, &load_base, &load_end);
            if (!CHECK_SUCCESS(status)) return status;
        }
    } else {
        fprintf(stderr, "%s: unsupported elf class\n", argv0);
        return STATUS_INVALID_VALUE;
    }
    if (load_base == UINTPTR_MAX || load_end <= load_base) return STATUS_INVALID_VALUE;

    program_size = load_end - load_base;
    LOG_DEBUG("offset=0x%p, size=%08zX\n", (void *)load_base, program_size);

    status = mm_allocate_pages_to(load_base / PAGE_SIZE, ALIGN_DIV(program_size, PAGE_SIZE));
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

    *load_paddr_out = (void *)load_base;
    *elf_out = elf;
    *program_size_out = program_size;

    return STATUS_SUCCESS;
}
