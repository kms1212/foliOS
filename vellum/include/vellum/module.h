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

status_t module_load(const char *path, struct module **mod);
void module_unload(struct module *mod);

struct module *module_get_first_mod(void);
status_t module_find(const char *name, struct module **mod);

#endif  // __VELLUM_MODULE_H__
