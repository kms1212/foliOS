#include <strata/elf.h>

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <strata/arch/elf.h>
#include <strata/arch/mmu_constants.h>

#include <strata/compiler.h>
#include <strata/macros.h>
#include <strata/mm.h>
#include <strata/mm/pool.h>
#include <strata/mm/types.h>
#include <strata/mm/utils.h>
#include <strata/status.h>

#define MODULE_NAME "elf"

static StStatus copy_from_img(
    struct StElf_Object *elf __in, size_t offset __in, void *buf __buf, size_t len __in
)
{
    if (offset + len > elf->img_size) return STATUS_INVALID_VALUE;

    memcpy(buf, (const char *)elf->img_base + offset, len);
    return STATUS_SUCCESS;
}

static StStatus copy_from_img_to_local(
    struct StElf_Object *elf __in,
    size_t offset __in,
    const struct StElf_LoadOptions *options __in,
    uintptr_t addr __in,
    size_t len __in
)
{
    assert(options);

    if (offset + len > elf->img_size) return STATUS_INVALID_VALUE;

    return StMm_WriteLocal(options->asp, addr, (const char *)elf->img_base + offset, len);
}

struct elf_load_segment {
    uintptr_t load_addr;
    St_PageCount page_count;
    size_t data_offset;
    size_t file_size;
    size_t mem_size;
    StMm_MapFlags map_flags;
};

static StStatus init_load_segment(
    uintptr_t load_addr __in,
    size_t data_offset __in,
    size_t file_size __in,
    size_t mem_size __in,
    uint32_t segment_flags __in,
    struct elf_load_segment *segment __out
)
{
    assert(segment);

    size_t page_offset = load_addr % PAGE_SIZE;

    if (file_size > mem_size) return STATUS_INVALID_VALUE;
    if (mem_size > SIZE_MAX - page_offset) return STATUS_INVALID_VALUE;

    segment->load_addr = load_addr;
    segment->page_count = (St_PageCount)ALIGN_DIV(page_offset + mem_size, PAGE_SIZE);
    segment->data_offset = data_offset;
    segment->file_size = file_size;
    segment->mem_size = mem_size;
    segment->map_flags = MF_USER_DEFAULT;

    if (!(segment_flags & PF_X)) {
        segment->map_flags |= MF_NO_EXECUTE;
    }

    if (!(segment_flags & PF_W)) {
        segment->map_flags &= ~MF_WRITABLE;
    }

    return STATUS_SUCCESS;
}

StStatus StElf_Open(
    const void *img_base __in, size_t img_size __in, struct StElf_Object **elfout __out
)
{
    assert(elfout);

    StStatus status;
    struct StElf_Object *elf = NULL;

    status = StPool_AllocateClear(sizeof(*elf), (void **)&elf);
    if (!CHECK_SUCCESS(status)) goto has_error;

    elf->img_base = img_base;
    elf->img_size = img_size;

    status = copy_from_img(elf, 0, &elf->ident, sizeof(elf->ident));
    if (!CHECK_SUCCESS(status)) {
        goto has_error;
    }

    if (memcmp(elf->ident.magic, ELFMAG, sizeof(elf->ident.magic)) != 0) {
        status = STATUS_INVALID_SIGNATURE;
        goto has_error;
    }
    if (elf->ident.endianness != ELFDATA2LSB) {
        status = STATUS_NOT_SUPPORTED;
        goto has_error;
    }
    if (elf->ident.header_version != EV_CURRENT) {
        status = STATUS_NOT_SUPPORTED;
        goto has_error;
    }

    if (elf->ident.class == ELFCLASS32) {
        status = copy_from_img(elf, 0, &elf->ehdr32, sizeof(elf->ehdr32));
        if (!CHECK_SUCCESS(status)) {
            goto has_error;
        }

        if (elf->ehdr32.machine != EM_386) {
            status = STATUS_NOT_SUPPORTED;
            goto has_error;
        }
    } else if (elf->ident.class == ELFCLASS64) {
        status = copy_from_img(elf, 0, &elf->ehdr64, sizeof(elf->ehdr64));
        if (!CHECK_SUCCESS(status)) {
            goto has_error;
        }

        if (elf->ehdr64.machine != EM_X86_64) {
            status = STATUS_NOT_SUPPORTED;
            goto has_error;
        }
    } else {
        status = STATUS_NOT_SUPPORTED;
        goto has_error;
    }

    *elfout = elf;

    return STATUS_SUCCESS;

has_error:
    if (elf) {
        StPool_Free(elf);
    }

    return status;
}

void StElf_Close(struct StElf_Object *elf __in)
{
    StPool_Free(elf);
}

StStatus StElf_GetHeader(struct StElf_Object *elf __in, void *buf __buf, size_t len __in)
{
    assert(elf);

    if (elf->ident.class == ELFCLASS32) {
        memcpy(buf, &elf->ehdr32, MIN(len, sizeof(elf->ehdr32)));
    } else if (elf->ident.class == ELFCLASS64) {
        memcpy(buf, &elf->ehdr64, MIN(len, sizeof(elf->ehdr64)));
    } else {
        return STATUS_NOT_SUPPORTED;
    }

    return STATUS_SUCCESS;
}

StStatus StElf_GetEntryPoint(struct StElf_Object *elf __in, uintptr_t *entry_point __out)
{
    assert(elf);
    assert(entry_point);

    if (elf->ident.class == ELFCLASS32) {
        *entry_point = (uintptr_t)elf->ehdr32.entry;
    } else if (elf->ident.class == ELFCLASS64) {
        *entry_point = (uintptr_t)elf->ehdr64.entry;
    } else {
        return STATUS_NOT_SUPPORTED;
    }

    return STATUS_SUCCESS;
}

StStatus StElf_GetProgramHeaderCount(struct StElf_Object *elf __in, unsigned int *count __out)
{
    assert(elf);
    assert(count);

    if (elf->ident.class == ELFCLASS32) {
        *count = elf->ehdr32.phnum;
    } else if (elf->ident.class == ELFCLASS64) {
        *count = elf->ehdr64.phnum;
    } else {
        return STATUS_NOT_SUPPORTED;
    }

    return STATUS_SUCCESS;
}

StStatus StElf_GetProgramHeader(
    struct StElf_Object *elf __in, unsigned int index __in, void *buf __buf, size_t len __in
)
{
    assert(elf);

    StStatus status;
    uint64_t phent_offset;
    size_t phent_size;

    if (elf->ident.class == ELFCLASS32) {
        if (index >= elf->ehdr32.phnum) return STATUS_INVALID_VALUE;

        phent_offset = elf->ehdr32.phoff + (StElf32_Off)(index * elf->ehdr32.phentsize);
        phent_size = MIN(len, elf->ehdr32.phentsize);
    } else if (elf->ident.class == ELFCLASS64) {
        if (index >= elf->ehdr64.phnum) return STATUS_INVALID_VALUE;

        phent_offset = elf->ehdr64.phoff + (StElf64_Off)(index * elf->ehdr64.phentsize);
        phent_size = MIN(len, elf->ehdr64.phentsize);
    } else {
        return STATUS_NOT_SUPPORTED;
    }

    status = copy_from_img(elf, phent_offset, buf, phent_size);
    if (!CHECK_SUCCESS(status)) {
        return status;
    }

    return STATUS_SUCCESS;
}

StStatus StElf_LoadProgram(
    struct StElf_Object *elf __in,
    unsigned int index __in,
    const struct StElf_LoadOptions *options __in
)
{
    StStatus status;
    struct StElf32_Phdr phdr32;
    struct StElf64_Phdr phdr64;
    struct elf_load_segment segment;
    struct StMm_ImageBacking backing;
    StMm_MapFlags map_flags;
    int allocated = 0;

    if (!elf || !options || !options->asp) return STATUS_INVALID_VALUE;
    if (options->flags & ~ELF_LOAD_MASK) return STATUS_INVALID_VALUE;

    if (elf->ident.class == ELFCLASS32) {
        status = StElf_GetProgramHeader(elf, index, &phdr32, sizeof(phdr32));
        if (!CHECK_SUCCESS(status)) goto has_error;

        if (phdr32.type != PT_LOAD) {
            return STATUS_INVALID_VALUE;
        }

        status = init_load_segment(
            (uintptr_t)phdr32.vaddr,
            (size_t)phdr32.offset,
            (size_t)phdr32.filesz,
            (size_t)phdr32.memsz,
            phdr32.flags,
            &segment
        );
        if (!CHECK_SUCCESS(status)) goto has_error;
    } else if (elf->ident.class == ELFCLASS64) {
        status = StElf_GetProgramHeader(elf, index, &phdr64, sizeof(phdr64));
        if (!CHECK_SUCCESS(status)) goto has_error;

        if (phdr64.type != PT_LOAD) {
            return STATUS_INVALID_VALUE;
        }

        status = init_load_segment(
            (uintptr_t)phdr64.vaddr,
            (size_t)phdr64.offset,
            (size_t)phdr64.filesz,
            (size_t)phdr64.memsz,
            phdr64.flags,
            &segment
        );
        if (!CHECK_SUCCESS(status)) goto has_error;
    } else {
        return STATUS_NOT_SUPPORTED;
    }

    if (segment.page_count == 0) return STATUS_SUCCESS;

    if (segment.data_offset > elf->img_size) return STATUS_INVALID_VALUE;
    if (segment.file_size > elf->img_size - segment.data_offset) return STATUS_INVALID_VALUE;

    // allocate page
    if (options->flags & ELF_LOAD_IMMEDIATE) {
        map_flags = segment.map_flags | MF_IMMEDIATE;
        status = StMm_AllocateLocalSparseTo(
            options->asp,
            ADDR_TO_PAGE(segment.load_addr),
            segment.page_count,
            options->alloc_flags,
            map_flags
        );
        if (!CHECK_SUCCESS(status)) goto has_error;
        allocated = 1;

        // copy program data
        status = copy_from_img_to_local(
            elf,
            segment.data_offset,
            options,
            segment.load_addr,
            segment.file_size
        );
        if (!CHECK_SUCCESS(status)) goto has_error;
    } else {
        backing.base = elf->img_base;
        backing.size = elf->img_size;
        backing.content_addr = segment.load_addr;
        backing.content_offset = segment.data_offset;
        backing.content_size = segment.file_size;

        status = StMm_AllocateLocalImageTo(
            options->asp,
            ADDR_TO_PAGE(segment.load_addr),
            segment.page_count,
            &backing,
            options->alloc_flags,
            segment.map_flags
        );
        if (!CHECK_SUCCESS(status)) goto has_error;
    }

    return STATUS_SUCCESS;

has_error:
    if (allocated) {
        StMm_FreeLocal(options->asp, ADDR_TO_PAGE(segment.load_addr), segment.page_count);
    }

    return status;
}
