#ifndef __STRATA_PLAT_TSS_H__
#define __STRATA_PLAT_TSS_H__

#include <stdint.h>

#include <strata/arch/tss.h>

void StP_InitTss(void);
void StP_SetTssStack(uintptr_t kstack);
struct StA_Tss *StP_GetTss(void);

#endif // __STRATA_PLAT_TSS_H__
