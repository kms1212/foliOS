#include <vellum/module.h>

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vellum/elf.h>
#include <vellum/log.h>
#include <vellum/macros.h>
#include <vellum/mm.h>
#include <vellum/panic.h>
#include <vellum/path.h>
#include <vellum/shell.h>
#include <vellum/status.h>

#include "symbol.h"

#define MODULE_NAME "module"

static status_t resolve_symbol_addr(
    struct elf_file *elf, struct elf32_sym *sym, uintptr_t load_vaddr, uintptr_t *addr
)
{
    status_t status;
    char *sym_name = NULL;

    sym_name = elf->strtab + sym->name;
    LOG_TRACE("finding symbol %s...\n", sym->name ? sym_name : "(no name)");

    if (sym->shndx != SHN_UNDEF) {
        if (addr) *addr = load_vaddr + sym->value;
        LOG_TRACE("symbol found at %08zX\n", *addr);
        return STATUS_SUCCESS;
    }

    void *sym_addr;
    status = _module_find_vellum_symbol(sym_name, &sym_addr);
    if (!CHECK_SUCCESS(status)) {
        LOG_DEBUG("symbol not found: %s\n", sym_name);
        return status;
    }

    if (addr) *addr = (uintptr_t)sym_addr;
    LOG_TRACE("symbol found at %08zX\n", *addr);
    return STATUS_SUCCESS;
}

static uint32_t resolve_symbol_value(struct elf_file *elf, unsigned int index)
{
    struct elf32_sym sym;
    status_t status;

    status = VlElf_GetSymbol(elf, index, &sym, sizeof(sym));
    if (!CHECK_SUCCESS(status)) return 0;

    return sym.value;
}

static status_t relocate_section(
    struct elf_file *elf,
    unsigned int rel_section_idx,
    uintptr_t got_symbol_offset,
    uintptr_t load_vaddr
)
{
    status_t status;
    struct elf32_shdr shdr;
    struct elf32_rel *rel_section = NULL;
    struct elf32_sym sym;
    size_t rel_section_size;
    unsigned int target_section_idx;
    int rel_type;
    unsigned int rel_sym_idx;
    uintptr_t P, A, S, B, GOT;

    B = load_vaddr;
    GOT = B + got_symbol_offset;

    /* load relocation section */
    status = VlElf_GetSectionHeader(elf, rel_section_idx, &shdr, sizeof(shdr));
    if (!CHECK_SUCCESS(status)) goto has_error;

    rel_section = malloc(shdr.size);
    rel_section_size = shdr.size;
    target_section_idx = shdr.info;

    status = VlElf_LoadSection(elf, rel_section_idx, rel_section, rel_section_size);
    if (!CHECK_SUCCESS(status)) goto has_error;

    /* get section offset */
    status = VlElf_GetSectionHeader(elf, target_section_idx, &shdr, sizeof(shdr));
    if (!CHECK_SUCCESS(status)) goto has_error;

    /* relocate */
    for (int i = 0; (i + 1) * sizeof(struct elf32_rel) <= rel_section_size; i++) {
        rel_type = ELF32_R_TYPE(rel_section[i].info);
        rel_sym_idx = ELF32_R_SYM(rel_section[i].info);

        P = load_vaddr + rel_section[i].offset;
        A = *(uint32_t *)P;

        if (rel_type != R_386_RELATIVE) {
            status = VlElf_GetSymbol(elf, rel_sym_idx, &sym, sizeof(sym));
            if (!CHECK_SUCCESS(status)) goto has_error;

            status = resolve_symbol_addr(elf, &sym, load_vaddr, &S);
            if (!CHECK_SUCCESS(status)) return status;
        }

        switch (rel_type) {
        case R_386_NONE:
            LOG_TRACE("type=%02d\n", rel_type);
            break;
        case R_386_32:
            // if (sym.shndx == SHN_UNDEF) {
            //     LOG_TRACE("type=%02d, (P:%08lX) = B:%08lX + A:%08lX\n", rel_type, P, B, A);
            //     *(uint32_t *)P = B + A;
            // } else {
            LOG_TRACE("type=%02d, (P:%08zX) = S:%08zX + A:%08zX\n", rel_type, P, S, A);
            *(uint32_t *)P = S + A;
            // }
            break;
        case R_386_PC32:
            if (sym.shndx == SHN_UNDEF) {
                LOG_TRACE("type=%02d, (P:%08zX) = S:%08zX + A:%08zX - P\n", rel_type, P, S, A);
                *(uint32_t *)P = S + A - P;
            }
            break;
        case R_386_GOT32:
        case R_386_GOT32X:
            LOG_TRACE(
                "type=%02d, (A:%08zX + GOT:%08zX) = (%08zX) = S:%08zX\n",
                rel_type,
                A,
                GOT,
                A + GOT,
                S
            );
            *(elf32_addr_t *)(A + GOT) = S;

            break;
        case R_386_GLOB_DAT:
        case R_386_JMP_SLOT:
            LOG_TRACE("type=%02d, (P:%08zX) = S:%08zX\n", rel_type, P, S);
            *(uint32_t *)P = S;
            break;
        case R_386_RELATIVE:
            LOG_TRACE("type=%02d, (P:%08zX) = B:%08zX + A:%08zX\n", rel_type, P, B, A);
            *(uint32_t *)P = B + A;
            break;
        case R_386_GOTOFF:
            if (sym.shndx == SHN_UNDEF) {
                LOG_TRACE(
                    "type=%02d, (P:%08zX) = S:%08zX + A:%08zX - GOT:%08zX\n",
                    rel_type,
                    P,
                    S,
                    A,
                    GOT
                );
                *(uint32_t *)P = S + A - GOT;
            }
            break;
        case R_386_GOTPC:
            if (sym.shndx == SHN_UNDEF) {
                LOG_TRACE("type=%02d, (P:%08zX) = GOT:%08zX + A:%08zX - P\n", rel_type, P, GOT, A);
                *(uint32_t *)P = GOT + A - P;
            }
            break;
        default:
            VlP_Panic(STATUS_NOT_SUPPORTED, "relocation type %02d not supported", rel_type);
            break;
        }

        LOG_TRACE("relocated to %08zX\n", *(uintptr_t *)P);
    }

    free(rel_section);

    return STATUS_SUCCESS;

has_error:
    if (rel_section) {
        free(rel_section);
    }

    return status;
}

struct note_vellum_entry {
    uint16_t type;
    uint16_t key_len;
    uint16_t value_len;
    uint16_t entry_len;
};

static struct module *mod_list_head = NULL;

static status_t run_func_array(struct elf_file *elf, const char *section_name, uintptr_t load_vaddr)
{
    status_t status;
    unsigned int section_idx;
    struct elf32_shdr shdr;
    size_t data_len;
    void (**func_arr)(void);

    status = VlElf_FindSection(elf, section_name, &section_idx);
    if (!CHECK_SUCCESS(status)) return status;

    status = VlElf_GetSectionHeader(elf, section_idx, &shdr, sizeof(shdr));
    if (!CHECK_SUCCESS(status)) return status;

    data_len = shdr.size;
    func_arr = (void (**)(void))(load_vaddr + shdr.address);
    for (unsigned long i = 0; i < data_len / sizeof(void (*)(void)); i++) {
        LOG_TRACE("running function at 0x%p\n", func_arr[i]);
        func_arr[i]();
    }

    return STATUS_SUCCESS;
}

status_t VlModule_Load(const char *path, struct module **modout)
{
    status_t status;
    struct elf_file *elf = NULL;
    struct elf32_ehdr ehdr;
    struct elf32_shdr shdr;
    struct elf32_phdr phdr;
    struct elf32_sym sym;
    size_t program_size = 0;
    vpn_t load_vpn = 0;
    unsigned int got_symbol_idx;
    uintptr_t got_symbol_offset;
    unsigned int rel_section_idx;
    unsigned int note_vellum_section_idx;
    void *note_vellum_section = NULL;
    uintptr_t note_vellum_offset;
    size_t note_vellum_section_len;
    struct module *mod = NULL;
    char *mod_name = NULL;
    status_t (*entry)(void) = NULL;

    status = VlElf_Open(path, &elf);
    if (!CHECK_SUCCESS(status)) goto has_error;

    status = VlElf_GetHeader(elf, &ehdr, sizeof(ehdr));
    if (!CHECK_SUCCESS(status)) goto has_error;

    if (ehdr.type != ET_DYN) {
        status = STATUS_NOT_SUPPORTED;
        goto has_error;
    }

    LOG_DEBUG("calculating program size...\n");
    for (int i = 0; i < ehdr.phnum; i++) {
        status = VlElf_GetProgramHeader(elf, i, &phdr, sizeof(phdr));
        if (!CHECK_SUCCESS(status)) goto has_error;

        if (phdr.type != PT_LOAD && phdr.type != PT_DYNAMIC) continue;

        if (program_size < phdr.vaddr + phdr.memsz) {
            program_size = phdr.vaddr + phdr.memsz;
        }
    }

    LOG_DEBUG("allocating program memory...\n");
    status = mm_allocate_pages(ALIGN_DIV(program_size, PAGE_SIZE), &load_vpn);
    if (!CHECK_SUCCESS(status)) return status;
    LOG_DEBUG("program memory allocated to page %zd\n", load_vpn);

    LOG_DEBUG("loading program...\n");
    for (int i = 0; i < ehdr.phnum; i++) {
        status = VlElf_GetProgramHeader(elf, i, &phdr, sizeof(phdr));
        if (!CHECK_SUCCESS(status)) goto has_error;

        if (phdr.type != PT_LOAD && phdr.type != PT_DYNAMIC) continue;

        status = VlElf_LoadProgram(elf, i, (void *)(load_vpn * PAGE_SIZE + phdr.vaddr));
        if (!CHECK_SUCCESS(status)) goto has_error;
    }

    LOG_DEBUG("getting offset of GOT...\n");
    status = VlElf_FindSymbol(elf, "_GLOBAL_OFFSET_TABLE_", &got_symbol_idx);
    if (!CHECK_SUCCESS(status)) goto has_error;

    status = VlElf_GetSymbol(elf, got_symbol_idx, &sym, sizeof(sym));
    if (!CHECK_SUCCESS(status)) goto has_error;

    got_symbol_offset = sym.value;

    static const char *rel_sections[] = {
        ".rel.dyn",
        ".rel.got",
        ".rel.text",
    };

    for (unsigned long i = 0; i < ARRAY_SIZE(rel_sections); i++) {
        LOG_DEBUG("relocating section %s...\n", rel_sections[i]);
        status = VlElf_FindSection(elf, rel_sections[i], &rel_section_idx);
        if (status == STATUS_ENTRY_NOT_FOUND) continue;
        if (!CHECK_SUCCESS(status)) goto has_error;

        status = relocate_section(elf, rel_section_idx, got_symbol_offset, load_vpn * PAGE_SIZE);
        if (!CHECK_SUCCESS(status)) goto has_error;
    }

    LOG_DEBUG("loading section .note.vellum...\n");
    status = VlElf_FindSection(elf, ".note.vellum", &note_vellum_section_idx);
    if (!CHECK_SUCCESS(status)) goto has_error;

    status = VlElf_GetSectionHeader(elf, note_vellum_section_idx, &shdr, sizeof(shdr));
    if (!CHECK_SUCCESS(status)) goto has_error;

    note_vellum_section_len = shdr.size;
    note_vellum_section = malloc(note_vellum_section_len);
    if (!note_vellum_section) {
        status = STATUS_UNKNOWN_ERROR;
        goto has_error;
    }

    status = VlElf_LoadSection(
        elf,
        note_vellum_section_idx,
        note_vellum_section,
        note_vellum_section_len
    );
    if (!CHECK_SUCCESS(status)) goto has_error;

    mod = malloc(sizeof(*mod));
    if (!mod) {
        status = STATUS_UNKNOWN_ERROR;
        goto has_error;
    }

    LOG_DEBUG("reading metadata...\n");
    note_vellum_offset = 0;
    while (note_vellum_offset < note_vellum_section_len) {
        struct note_vellum_entry *entry =
            (void *)((uintptr_t)note_vellum_section + note_vellum_offset);
        char *key = (char *)((uintptr_t)entry + sizeof(struct note_vellum_entry));
        void *value = key + entry->key_len + 1;

        if (strncmp("name", key, entry->key_len) == 0) {
            if (entry->type != 1) {
                status = STATUS_INVALID_VALUE;
                goto has_error;
            }

            mod_name = strndup(value, entry->value_len + 1);
            break;
        }

        note_vellum_offset += entry->entry_len;
    }
    if (!mod_name) {
        status = STATUS_INVALID_VALUE;
        goto has_error;
    }

    mod->elf = elf;
    mod->name = mod_name;
    mod->load_vpn = load_vpn;
    mod->program_size = program_size;

    free(note_vellum_section);
    note_vellum_section = NULL;

    LOG_DEBUG("executing constructor functions... %zu\n", load_vpn);
    status = run_func_array(elf, ".init_array", load_vpn * PAGE_SIZE);
    if (!CHECK_SUCCESS(status)) goto has_error;

    LOG_DEBUG("executing entry point...\n");
    entry = (void *)(load_vpn * PAGE_SIZE + ehdr.entry);
    if (ehdr.entry) {
        status = entry();
        if (!CHECK_SUCCESS(status)) goto has_error;
    }

    if (!mod_list_head) {
        mod_list_head = mod;
    } else {
        struct module *current = mod_list_head;
        for (; current->next; current = current->next) {
        }

        current->next = mod;
    }

    mod->next = NULL;

    if (modout) *modout = mod;

    return STATUS_SUCCESS;

has_error:
    if (entry) {
        run_func_array(elf, ".fini_array", load_vpn * PAGE_SIZE);
    }

    if (mod_name) {
        free(mod_name);
    }

    if (note_vellum_section) {
        free(note_vellum_section);
    }

    if (mod) {
        free(mod);
    }

    if (load_vpn) {
        mm_free_pages(load_vpn, ALIGN_DIV(program_size, PAGE_SIZE));
    }

    if (elf) {
        VlElf_Close(elf);
    }

    return status;
}

void VlModule_Unload(struct module *mod)
{
    struct elf_file *elf;
    vpn_t load_vpn;
    size_t program_size;

    if (!mod_list_head) return;

    struct module *prev_mod = NULL;
    for (struct module *current = mod_list_head; current->next; current = current->next) {
        if (current->next == mod) {
            prev_mod = current;
        }
    }
    if (!prev_mod) return;

    prev_mod->next = mod->next;

    run_func_array(mod->elf, ".fini_array", mod->load_vpn * PAGE_SIZE);

    elf = mod->elf;
    load_vpn = mod->load_vpn;
    program_size = mod->program_size;

    free(mod->name);
    free(mod);

    mm_free_pages(load_vpn, ALIGN_DIV(program_size, PAGE_SIZE));

    VlElf_Close(elf);
}

struct module *VlModule_GetFirst(void)
{
    return mod_list_head;
}

status_t VlModule_Find(const char *name, struct module **mod);
