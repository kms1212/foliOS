#include <strata/plat/cpulocal.h>

#include <stddef.h>
#include <stdint.h>

#include <strata/arch/intrinsics/msr.h>

#include <strata/compiler.h>
#include <strata/mm/address_space_refs.h>
#include <strata/status.h>

static struct StCpuLocalP_Data bsp_data;
__externally_visible int _cpulocal_initialized = 0;

extern char _early_stack[];
extern struct StAddressSpace base_asp;

StStatus StCpuLocalP_Init(void)
{
    bsp_data.cpu_id = 0;
    bsp_data.is_bsp = 1;
    bsp_data.self = &bsp_data;
    bsp_data.kernel_rsp = (uintptr_t)_early_stack;
    bsp_data.current_asp = (StAddressSpace_InternalRef)&base_asp;

    StA_WriteMsr(MSR_GS_BASE, (uintptr_t)&bsp_data);

    _cpulocal_initialized = 1;

    return STATUS_SUCCESS;
}
