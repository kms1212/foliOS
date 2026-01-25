#ifndef __STRATA_PLAT_GDT_H__
#define __STRATA_PLAT_GDT_H__

#include <strata/arch/gdt.h>

#define GDT_ENTRY_COUNT 6

#define SEG_SEL_KERNEL_CODE 0x08
#define SEG_SEL_KERNEL_DATA 0x10
#define SEG_SEL_USER_CODE   0x18
#define SEG_SEL_USER_DATA   0x20
#define SEG_SEL_TSS         0x28

void StP_InitGdt(void);

#endif  // __STRATA_PLAT_GDT_H__
