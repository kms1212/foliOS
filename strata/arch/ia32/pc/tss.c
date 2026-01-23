#include <strata/plat/tss.h>

#include <string.h>

#include <strata/plat/gdt.h>

struct StA_Tss _pc_tss;

void StP_InitTss(void)
{
    memset(&_pc_tss, 0, sizeof(_pc_tss));
    _pc_tss.ss0 = SEG_SEL_KERNEL_DATA;
    _pc_tss.iomap_base = sizeof(_pc_tss);
}

void StP_SetTssStack(uintptr_t kstack)
{
    _pc_tss.esp0 = kstack;
}
