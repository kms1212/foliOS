#ifndef __VELLUM_MODULE_H__
#define __VELLUM_MODULE_H__

#include <vellum/elf.h>
#include <vellum/mm.h>
#include <vellum/status.h>

/**
 * Loaded Vellum module.
 *
 * Modules are ELF files loaded by the bootloader shell and linked into the
 * global module list. `load_folios` is one such module: it gathers bootloader state,
 * builds the Strata bootinfo table, and transfers control to the kernel.
 */
struct module {
    /** Intrusive link in Vellum's global module list. */
    struct module *next;

    /** Parsed ELF backing file owned by the module. */
    struct elf_file *elf;

    /** Module name used for later lookup. */
    char *name;

    /** First virtual page used for the loaded program image. */
    vpn_t load_vpn;

    /** Size in bytes of the loaded program image. */
    size_t program_size;
};

/**
 * Loads an ELF module from the active filesystem.
 *
 * On success, `mod` receives a module owned by the global module list. Call
 * `VlModule_Unload` to remove it explicitly.
 */
VlStatus VlModule_Load(const char *path, struct module **mod);

/**
 * Unloads a module and releases its ELF file and mapped image.
 */
void VlModule_Unload(struct module *mod);

/**
 * Returns the first module in the global module list, or NULL if none exist.
 */
struct module *VlModule_GetFirst(void);

/**
 * Finds a loaded module by name.
 */
VlStatus VlModule_Find(const char *name, struct module **mod);

#endif  // __VELLUM_MODULE_H__
