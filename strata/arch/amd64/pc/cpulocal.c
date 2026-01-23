#include <strata/plat/cpulocal.h>

#include <strata/arch/intrinsics/msr.h>

#include <strata/log.h>

static struct StCpuLocalP_Data bsp_data;

extern int _early_stack;

StStatus StCpuLocalP_Init(void)
{
    bsp_data.cpu_id = 0;
    bsp_data.is_bsp = 1;
    bsp_data.self = &bsp_data;
    bsp_data.kernel_rsp = (uintptr_t)&_early_stack;

    StA_WriteMsr(MSR_GS_BASE, (uintptr_t)&bsp_data);

    return STATUS_SUCCESS;
}
