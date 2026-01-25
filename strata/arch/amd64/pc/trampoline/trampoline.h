#ifndef __TRAMPOLINE_H__
#define __TRAMPOLINE_H__

#include <strata/mm.h>

void setup_trampoline_page_tables(void);
St_PhysFrame get_trampoline_pml4_phys(void);

#endif  // __TRAMPOLINE_H__
