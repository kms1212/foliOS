#include <strata/plat/syscall.h>

#include <inttypes.h>

#include <strata/arch/cpufeatures.h>
#include <strata/arch/intrinsics/msr.h>
#include <strata/arch/interrupt.h>

#include <strata/plat/gdt.h>
#include <strata/plat/interrupt.h>

#include <strata/status.h>
#include <strata/log.h>

#define MODULE_NAME "syscall"

extern void _StSyscallP_Entry(void);

__externally_visible
void _StSyscallP_Handler(struct StA_InterruptFrame *frame, struct StIntP_Context *ctx)
{
    LOG_DEBUG("rax: 0x%016"PRIX64"\n", ctx->rax);
}

StStatus StSyscallP_Init(void)
{
    if (!g_p_cpu_features->has_syscall) return STATUS_UNSUPPORTED;

    StA_WriteMsr(MSR_LSTAR, (uintptr_t)_StSyscallP_Entry);
    StA_WriteMsr(MSR_STAR, ((uint64_t)SEG_SEL_KERNEL_CODE << 32) | (((uint64_t)SEG_SEL_USER_DATA - 8) << 48));
    StA_WriteMsr(MSR_SFMASK, 0x0000000000000202);

    return STATUS_SUCCESS;
}

