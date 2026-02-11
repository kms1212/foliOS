#include <strata/elf.h>

#include <stdlib.h>
#include <string.h>

#include <strata/log.h>
#include <strata/macros.h>
#include <strata/mm.h>

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
    struct StMm_AddressSpace *asp __in,
    uintptr_t addr __in,
    size_t len __in
)
{
    if (offset + len > elf->img_size) return STATUS_INVALID_VALUE;

    return StMm_WriteLocal(asp, addr, (const char *)elf->img_base + offset, len);
}

StStatus StElf_Open(
    const void *img_base __in, size_t img_size __in, struct StElf_Object **elfout __out
)
{
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
        status = STATUS_UNSUPPORTED;
        goto has_error;
    }
    if (elf->ident.header_version != EV_CURRENT) {
        status = STATUS_UNSUPPORTED;
        goto has_error;
    }

    if (elf->ident.class == ELFCLASS32) {
        status = copy_from_img(elf, 0, &elf->ehdr32, sizeof(elf->ehdr32));
        if (!CHECK_SUCCESS(status)) {
            goto has_error;
        }

        if (elf->ehdr32.machine != EM_386) {
            status = STATUS_UNSUPPORTED;
            goto has_error;
        }
    } else if (elf->ident.class == ELFCLASS64) {
        status = copy_from_img(elf, 0, &elf->ehdr64, sizeof(elf->ehdr64));
        if (!CHECK_SUCCESS(status)) {
            goto has_error;
        }

        if (elf->ehdr64.machine != EM_X86_64) {
            status = STATUS_UNSUPPORTED;
            goto has_error;
        }
    } else {
        status = STATUS_UNSUPPORTED;
        goto has_error;
    }

    if (elfout) *elfout = elf;

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
    if (elf->ident.class == ELFCLASS32) {
        memcpy(buf, &elf->ehdr32, MIN(len, sizeof(elf->ehdr32)));
    } else if (elf->ident.class == ELFCLASS64) {
        memcpy(buf, &elf->ehdr64, MIN(len, sizeof(elf->ehdr64)));
    } else {
        return STATUS_UNSUPPORTED;
    }

    return STATUS_SUCCESS;
}

StStatus StElf_GetEntryPoint(struct StElf_Object *elf __in, uintptr_t *entry_point __out)
{
    if (elf->ident.class == ELFCLASS32) {
        *entry_point = (uintptr_t)elf->ehdr32.entry;
    } else if (elf->ident.class == ELFCLASS64) {
        *entry_point = (uintptr_t)elf->ehdr64.entry;
    } else {
        return STATUS_UNSUPPORTED;
    }

    return STATUS_SUCCESS;
}

StStatus StElf_GetProgramHeaderCount(struct StElf_Object *elf __in, unsigned int *count __out)
{
    if (elf->ident.class == ELFCLASS32) {
        *count = elf->ehdr32.phnum;
    } else if (elf->ident.class == ELFCLASS64) {
        *count = elf->ehdr64.phnum;
    } else {
        return STATUS_UNSUPPORTED;
    }

    return STATUS_SUCCESS;
}

StStatus StElf_GetProgramHeader(
    struct StElf_Object *elf __in, unsigned int index __in, void *buf __buf, size_t len __in
)
{
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
        return STATUS_UNSUPPORTED;
    }

    status = copy_from_img(elf, phent_offset, buf, phent_size);
    if (!CHECK_SUCCESS(status)) {
        return status;
    }

    return STATUS_SUCCESS;
}

StStatus StElf_LoadProgram(
    struct StElf_Object *elf __in, unsigned int index __in, struct StMm_AddressSpace *asp __in
)
{
    StStatus status;
    struct StElf32_Phdr phdr32;
    struct StElf64_Phdr phdr64;
    uintptr_t program_load_addr;
    size_t program_size_page;
    uintptr_t program_data_offset;
    size_t program_memsz;
    size_t program_filesz;
    int allocated = 0;
    uint32_t map_flags = MF_USER_DEFAULT;

    if (elf->ident.class == ELFCLASS32) {
        status = StElf_GetProgramHeader(elf, index, &phdr32, sizeof(phdr32));
        if (!CHECK_SUCCESS(status)) goto has_error;

        if (phdr32.type != PT_LOAD) {
            return STATUS_INVALID_VALUE;
        }

        program_load_addr = phdr32.vaddr;
        program_size_page = ALIGN_DIV(program_load_addr % PAGE_SIZE + phdr32.memsz, PAGE_SIZE);
        program_data_offset = phdr32.offset;
        program_memsz = phdr32.memsz;
        program_filesz = phdr32.filesz;

        if (!(phdr32.flags & PF_X)) {
            map_flags |= MF_NO_EXECUTE;
        }

        if (!(phdr32.flags & PF_W)) {
            map_flags &= ~MF_WRITABLE;
        }
    } else if (elf->ident.class == ELFCLASS64) {
        status = StElf_GetProgramHeader(elf, index, &phdr64, sizeof(phdr64));
        if (!CHECK_SUCCESS(status)) goto has_error;

        if (phdr64.type != PT_LOAD) {
            return STATUS_INVALID_VALUE;
        }

        program_load_addr = phdr64.vaddr;
        program_size_page = ALIGN_DIV(program_load_addr % PAGE_SIZE + phdr64.memsz, PAGE_SIZE);
        program_data_offset = phdr64.offset;
        program_memsz = phdr64.memsz;
        program_filesz = phdr64.filesz;

        if (!(phdr64.flags & PF_X)) {
            map_flags |= MF_NO_EXECUTE;
        }

        if (!(phdr64.flags & PF_W)) {
            map_flags &= ~MF_WRITABLE;
        }
    } else {
        return STATUS_UNSUPPORTED;
    }

    // allocate page
    status = StMm_AllocateLocalSparseTo(
        asp,
        ADDR_TO_PAGE(program_load_addr),
        program_size_page,
        AF_DEFAULT,
        MF_USER_DEFAULT & ~MF_USER
    );
    if (!CHECK_SUCCESS(status)) goto has_error;
    allocated = 1;

    // copy program data
    status =
        copy_from_img_to_local(elf, program_data_offset, asp, program_load_addr, program_filesz);
    if (!CHECK_SUCCESS(status)) goto has_error;

    // zero-fill if needed
    if (program_memsz > program_filesz) {
        status = StMm_SetLocal(
            asp,
            program_load_addr + program_filesz,
            0,
            program_memsz - program_filesz
        );
        if (!CHECK_SUCCESS(status)) goto has_error;
    }

    // fixup page map flags
    status =
        StMm_SetLocalPageFlags(asp, ADDR_TO_PAGE(program_load_addr), program_size_page, map_flags);
    if (!CHECK_SUCCESS(status)) goto has_error;

    return STATUS_SUCCESS;

has_error:
    if (allocated) {
        StMm_FreeLocal(asp, ADDR_TO_PAGE(program_load_addr), program_size_page);
    }

    return status;
}
