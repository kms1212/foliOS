#ifndef __VELLUM_MODULE_H__
#define __VELLUM_MODULE_H__

#include <vellum/elf.h>
#include <vellum/mm.h>
#include <vellum/status.h>

struct module {
    struct module *next;

    struct elf_file *elf;

    char *name;
    vpn_t load_vpn;
    size_t program_size;
};

status_t VlModule_Load(const char *path, struct module **mod);
void VlModule_Unload(struct module *mod);

struct module *VlModule_GetFirst(void);
status_t VlModule_Find(const char *name, struct module **mod);

#endif  // __VELLUM_MODULE_H__
