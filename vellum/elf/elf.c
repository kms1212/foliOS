#include <vellum/elf.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vellum/log.h>
#include <vellum/macros.h>
#include <vellum/mm.h>

#define MODULE_NAME "elf"

status_t VlElf_Open(const char *path, struct elf_file **elfout)
{
    status_t status;
    struct elf_file *elf = NULL;
    struct elf32_shdr shdr32;
    struct elf64_shdr shdr64;
    unsigned int strtab_idx = -1;
    unsigned int symtab_idx = -1;

    elf = calloc(1, sizeof(struct elf_file));
    if (!elf) {
        status = STATUS_UNKNOWN_ERROR;
        goto has_error;
    }

    elf->fp = fopen(path, "rb");
    if (!elf->fp) {
        status = STATUS_UNKNOWN_ERROR;
        goto has_error;
    }

    fseek(elf->fp, 0, SEEK_SET);
    fread(&elf->ident, sizeof(elf->ident), 1, elf->fp);

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
        fseek(elf->fp, 0, SEEK_SET);
        fread(&elf->ehdr32, sizeof(elf->ehdr32), 1, elf->fp);

        if (elf->ehdr32.machine != EM_386) {
            status = STATUS_NOT_SUPPORTED;
            goto has_error;
        }

        status = VlElf_GetSectionHeader(elf, elf->ehdr32.shstrndx, &shdr32, sizeof(shdr32));
        if (!CHECK_SUCCESS(status)) goto has_error;

        elf->shstrtab = malloc(shdr32.size);
        status = VlElf_LoadSection(elf, elf->ehdr32.shstrndx, elf->shstrtab, shdr32.size);
        if (!CHECK_SUCCESS(status)) goto has_error;

        status = VlElf_FindSection(elf, ".strtab", &strtab_idx);
        if (!CHECK_SUCCESS(status)) goto has_error;

        status = VlElf_GetSectionHeader(elf, strtab_idx, &shdr32, sizeof(shdr32));
        if (!CHECK_SUCCESS(status)) goto has_error;

        elf->strtab = malloc(shdr32.size);
        status = VlElf_LoadSection(elf, strtab_idx, elf->strtab, shdr32.size);
        if (!CHECK_SUCCESS(status)) goto has_error;

        status = VlElf_FindSection(elf, ".symtab", &symtab_idx);
        if (!CHECK_SUCCESS(status)) goto has_error;

        status = VlElf_GetSectionHeader(elf, symtab_idx, &shdr32, sizeof(shdr32));
        if (!CHECK_SUCCESS(status)) goto has_error;

        elf->symtab32 = malloc(shdr32.size);
        elf->symtab_size = shdr32.size;

        status = VlElf_LoadSection(elf, symtab_idx, elf->symtab32, elf->symtab_size);
        if (!CHECK_SUCCESS(status)) goto has_error;
    } else if (elf->ident.class == ELFCLASS64) {
        fseek(elf->fp, 0, SEEK_SET);
        fread(&elf->ehdr64, sizeof(elf->ehdr64), 1, elf->fp);

        if (elf->ehdr64.machine != EM_X86_64) {
            status = STATUS_NOT_SUPPORTED;
            goto has_error;
        }

        status = VlElf_GetSectionHeader(elf, elf->ehdr64.shstrndx, &shdr64, sizeof(shdr64));
        if (!CHECK_SUCCESS(status)) goto has_error;

        elf->shstrtab = malloc(shdr64.size);
        status = VlElf_LoadSection(elf, elf->ehdr64.shstrndx, elf->shstrtab, shdr64.size);
        if (!CHECK_SUCCESS(status)) goto has_error;

        status = VlElf_FindSection(elf, ".strtab", &strtab_idx);
        if (!CHECK_SUCCESS(status)) goto has_error;

        status = VlElf_GetSectionHeader(elf, strtab_idx, &shdr64, sizeof(shdr64));
        if (!CHECK_SUCCESS(status)) goto has_error;

        elf->strtab = malloc(shdr64.size);
        status = VlElf_LoadSection(elf, strtab_idx, elf->strtab, shdr64.size);
        if (!CHECK_SUCCESS(status)) goto has_error;

        status = VlElf_FindSection(elf, ".symtab", &symtab_idx);
        if (!CHECK_SUCCESS(status)) goto has_error;

        status = VlElf_GetSectionHeader(elf, symtab_idx, &shdr64, sizeof(shdr64));
        if (!CHECK_SUCCESS(status)) goto has_error;

        elf->symtab64 = malloc(shdr64.size);
        elf->symtab_size = shdr64.size;

        status = VlElf_LoadSection(elf, symtab_idx, elf->symtab64, elf->symtab_size);
        if (!CHECK_SUCCESS(status)) goto has_error;
    } else {
        status = STATUS_NOT_SUPPORTED;
        goto has_error;
    }

    if (elfout) *elfout = elf;

    return STATUS_SUCCESS;

has_error:
    if (elf && elf->strtab) {
        free(elf->strtab);
    }

    if (elf && elf->shstrtab) {
        free(elf->shstrtab);
    }

    if (elf && elf->fp) {
        fclose(elf->fp);
    }

    if (elf) {
        free(elf);
    }

    return status;
}

void VlElf_Close(struct elf_file *elf)
{
    fclose(elf->fp);
    free(elf);
}

status_t VlElf_GetHeader(struct elf_file *elf, void *buf, size_t len)
{
    if (elf->ident.class == ELFCLASS32) {
        memcpy(buf, &elf->ehdr32, MIN(len, sizeof(elf->ehdr32)));
    } else if (elf->ident.class == ELFCLASS64) {
        memcpy(buf, &elf->ehdr64, MIN(len, sizeof(elf->ehdr64)));
    } else {
        return STATUS_NOT_SUPPORTED;
    }

    return STATUS_SUCCESS;
}

status_t VlElf_GetProgramHeader(struct elf_file *elf, unsigned int index, void *buf, size_t len)
{
    uint64_t phent_offset;
    size_t phent_size;

    if (elf->ident.class == ELFCLASS32) {
        if (index >= elf->ehdr32.phnum) return STATUS_INVALID_VALUE;

        phent_offset = elf->ehdr32.phoff + index * elf->ehdr32.phentsize;
        phent_size = MIN(len, elf->ehdr32.phentsize);
    } else if (elf->ident.class == ELFCLASS64) {
        if (index >= elf->ehdr64.phnum) return STATUS_INVALID_VALUE;

        phent_offset = elf->ehdr64.phoff + (elf64_off_t)(index * elf->ehdr64.phentsize);
        phent_size = MIN(len, elf->ehdr64.phentsize);
    } else {
        return STATUS_NOT_SUPPORTED;
    }

    fseek(elf->fp, (long)phent_offset, SEEK_SET);
    fread(buf, phent_size, 1, elf->fp);

    return STATUS_SUCCESS;
}

status_t VlElf_LoadProgram(struct elf_file *elf, unsigned int index, void *paddr)
{
    status_t status;
    struct elf32_phdr phdr32;
    struct elf64_phdr phdr64;
    uintptr_t program_load_addr;
    size_t program_size_page;
    long program_data_offset;
    size_t program_memsz;
    size_t program_filesz;

    if (elf->ident.class == ELFCLASS32) {
        status = VlElf_GetProgramHeader(elf, index, &phdr32, sizeof(phdr32));
        if (!CHECK_SUCCESS(status)) return status;

        program_load_addr = paddr ? (uintptr_t)paddr : phdr32.paddr;
        program_size_page = ALIGN_DIV(program_load_addr % PAGE_SIZE + phdr32.memsz, PAGE_SIZE);
        program_data_offset = (long)phdr32.offset;
        program_memsz = phdr32.memsz;
        program_filesz = phdr32.filesz;
    } else if (elf->ident.class == ELFCLASS64) {
        status = VlElf_GetProgramHeader(elf, index, &phdr64, sizeof(phdr64));
        if (!CHECK_SUCCESS(status)) return status;

        program_load_addr = paddr ? (uintptr_t)paddr : phdr64.paddr;
        program_size_page = ALIGN_DIV(program_load_addr % PAGE_SIZE + phdr64.memsz, PAGE_SIZE);
        program_data_offset = (long)phdr64.offset;
        program_memsz = phdr64.memsz;
        program_filesz = phdr64.filesz;
    } else {
        return STATUS_NOT_SUPPORTED;
    }

    status = mm_allocate_pages_to(program_load_addr / PAGE_SIZE, program_size_page);
    if (!CHECK_SUCCESS(status)) return status;

    fseek(elf->fp, program_data_offset, SEEK_SET);
    fread((void *)program_load_addr, program_filesz, 1, elf->fp);

    if (program_memsz > program_filesz) {
        memset((void *)(program_load_addr + program_filesz), 0, program_memsz - program_filesz);
    }

    return STATUS_SUCCESS;
}

status_t VlElf_GetSectionHeader(struct elf_file *elf, unsigned int index, void *buf, size_t len)
{
    if (elf->ident.class == ELFCLASS32) {
        if (index >= elf->ehdr32.shnum) return STATUS_INVALID_VALUE;

        fseek(elf->fp, (int)(elf->ehdr32.shoff + index * elf->ehdr32.shentsize), SEEK_SET);
        fread(buf, MIN(len, elf->ehdr32.shentsize), 1, elf->fp);
    } else if (elf->ident.class == ELFCLASS64) {
        if (index >= elf->ehdr64.shnum) return STATUS_INVALID_VALUE;

        fseek(
            elf->fp,
            (long)(elf->ehdr64.shoff + (elf64_off_t)(index * elf->ehdr64.shentsize)),
            SEEK_SET
        );
        fread(buf, MIN(len, elf->ehdr64.shentsize), 1, elf->fp);
    } else {
        return STATUS_NOT_SUPPORTED;
    }

    return STATUS_SUCCESS;
}

status_t VlElf_GetSectionName(struct elf_file *elf, unsigned int index, const char **name)
{
    status_t status;
    struct elf32_shdr shdr32;
    struct elf32_shdr shdr64;

    if (elf->ident.class == ELFCLASS32) {
        status = VlElf_GetSectionHeader(elf, index, &shdr32, sizeof(shdr32));
        if (!CHECK_SUCCESS(status)) return status;

        if (name) *name = elf->shstrtab + shdr32.name;
    } else if (elf->ident.class == ELFCLASS64) {
        status = VlElf_GetSectionHeader(elf, index, &shdr64, sizeof(shdr64));
        if (!CHECK_SUCCESS(status)) return status;

        if (name) *name = elf->shstrtab + shdr64.name;
    } else {
        return STATUS_NOT_SUPPORTED;
    }

    return STATUS_SUCCESS;
}

status_t VlElf_FindSection(struct elf_file *elf, const char *name, unsigned int *idx)
{
    const char *section_name;
    status_t status;

    if (elf->ident.class == ELFCLASS32) {
        for (int i = 0; i < elf->ehdr32.shnum; i++) {
            status = VlElf_GetSectionName(elf, i, &section_name);
            if (!CHECK_SUCCESS(status)) return status;

            if (strcmp(section_name, name) == 0) {
                if (idx) *idx = i;
                return STATUS_SUCCESS;
            }
        }
    } else if (elf->ident.class == ELFCLASS64) {
        for (int i = 0; i < elf->ehdr64.shnum; i++) {
            status = VlElf_GetSectionName(elf, i, &section_name);
            if (!CHECK_SUCCESS(status)) return status;

            if (strcmp(section_name, name) == 0) {
                if (idx) *idx = i;
                return STATUS_SUCCESS;
            }
        }
    } else {
        return STATUS_NOT_SUPPORTED;
    }

    return STATUS_ENTRY_NOT_FOUND;
}

status_t VlElf_LoadSection(struct elf_file *elf, unsigned int index, void *buf, size_t len)
{
    status_t status;
    struct elf32_shdr shdr32;
    struct elf64_shdr shdr64;

    if (elf->ident.class == ELFCLASS32) {
        status = VlElf_GetSectionHeader(elf, index, &shdr32, sizeof(shdr32));
        if (!CHECK_SUCCESS(status)) return status;

        fseek(elf->fp, (int)shdr32.offset, SEEK_SET);
        fread(buf, MIN(shdr32.size, len), 1, elf->fp);
    } else if (elf->ident.class == ELFCLASS64) {
        status = VlElf_GetSectionHeader(elf, index, &shdr64, sizeof(shdr64));
        if (!CHECK_SUCCESS(status)) return status;

        fseek(elf->fp, (long)shdr64.offset, SEEK_SET);
        fread(buf, MIN(shdr64.size, len), 1, elf->fp);
    } else {
        return STATUS_NOT_SUPPORTED;
    }

    return STATUS_SUCCESS;
}

status_t VlElf_FindSymbol(struct elf_file *elf, const char *name, unsigned int *index)
{
    const char *sym_name;

    if (elf->ident.class == ELFCLASS32) {
        for (int i = 0; (i + 1) * sizeof(*elf->symtab32) <= elf->symtab_size; i++) {
            sym_name = elf->strtab + elf->symtab32[i].name;

            if (strcmp(name, sym_name) == 0) {
                if (index) *index = i;
                return STATUS_SUCCESS;
            }
        }
    } else if (elf->ident.class == ELFCLASS64) {
        for (int i = 0; (i + 1) * sizeof(*elf->symtab64) <= elf->symtab_size; i++) {
            sym_name = elf->strtab + elf->symtab64[i].name;

            if (strcmp(name, sym_name) == 0) {
                if (index) *index = i;
                return STATUS_SUCCESS;
            }
        }
    } else {
        return STATUS_NOT_SUPPORTED;
    }

    return STATUS_ENTRY_NOT_FOUND;
}

status_t VlElf_GetSymbol(struct elf_file *elf, unsigned int index, void *buf, size_t len)
{
    if (elf->ident.class != ELFCLASS32) return STATUS_NOT_SUPPORTED;

    if (elf->ident.class == ELFCLASS32) {
        if (index * sizeof(*elf->symtab32) >= elf->symtab_size) {
            return STATUS_INVALID_VALUE;
        }

        memcpy(buf, &elf->symtab32[index], MIN(len, sizeof(*elf->symtab32)));
    } else if (elf->ident.class == ELFCLASS64) {
        if (index * sizeof(*elf->symtab64) >= elf->symtab_size) {
            return STATUS_INVALID_VALUE;
        }

        memcpy(buf, &elf->symtab64[index], MIN(len, sizeof(*elf->symtab64)));
    } else {
        return STATUS_NOT_SUPPORTED;
    }

    return STATUS_SUCCESS;
}

status_t VlElf_GetSymbolCount(struct elf_file *elf, unsigned int *count)
{
    if (elf->ident.class != ELFCLASS32) return STATUS_NOT_SUPPORTED;

    if (elf->ident.class == ELFCLASS32) {
        if (count) *count = elf->symtab_size / sizeof(*elf->symtab32);
    } else if (elf->ident.class == ELFCLASS64) {
        if (count) *count = elf->symtab_size / sizeof(*elf->symtab64);
    } else {
        return STATUS_NOT_SUPPORTED;
    }

    return STATUS_SUCCESS;
}
