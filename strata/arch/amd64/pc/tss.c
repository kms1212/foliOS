#include <strata/plat/tss.h>

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <strata/arch/tss.h>

static struct StA_Tss _pc_tss;

void StP_InitTss(void)
{
    memset(&_pc_tss, 0, sizeof(_pc_tss));
    _pc_tss.iomap_base = sizeof(_pc_tss);
}

struct StA_Tss *StP_GetTss(void)
{
    return &_pc_tss;
}

void StP_SetTssStack(uintptr_t kstack)
{
    _pc_tss.rsp0_high = (kstack >> 32) & 0xFFFFFFFF;
    _pc_tss.rsp0_low = kstack & 0xFFFFFFFF;
}
