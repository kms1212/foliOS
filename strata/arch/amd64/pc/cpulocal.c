#include <strata/plat/cpulocal.h>

#include <strata/arch/intrinsics/msr.h>

#include <strata/plat/mm.h>

#include <strata/compiler.h>
#include <strata/log.h>

static struct StCpuLocalP_Data bsp_data;
__externally_visible int _cpulocal_initialized = 0;

extern int _early_stack;
extern struct StMm_AddressSpace base_asp;

StStatus StCpuLocalP_Init(void)
{
    bsp_data.cpu_id = 0;
    bsp_data.is_bsp = 1;
    bsp_data.self = &bsp_data;
    bsp_data.kernel_rsp = (uintptr_t)&_early_stack;
    bsp_data.current_asp = &base_asp;

    StA_WriteMsr(MSR_GS_BASE, (uintptr_t)&bsp_data);

    _cpulocal_initialized = 1;

    return STATUS_SUCCESS;
}
