#ifndef __VELLUM_PLAT_GDT_H__
#define __VELLUM_PLAT_GDT_H__

#include <vellum/arch/gdt.h>

#include <vellum/status.h>

void _pc_gdt_init(void);

extern struct StA_GdtEntry _pc_gdt[8192];

#endif  // __VELLUM_PLAT_GDT_H__
