#ifndef __VELLUM_ASM_PC_GDT_H__
#define __VELLUM_ASM_PC_GDT_H__

#include <vellum/asm/gdt.h>

#include <vellum/status.h>

void _pc_gdt_init(void);

extern struct gdt_entry _pc_gdt[8192];

#endif // __VELLUM_ASM_PC_GDT_H__
