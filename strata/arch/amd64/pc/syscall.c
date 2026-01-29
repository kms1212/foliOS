#include <strata/plat/syscall.h>

#include <inttypes.h>

#include <strata/arch/cpufeatures.h>
#include <strata/arch/interrupt.h>
#include <strata/arch/intrinsics/msr.h>

#include <strata/plat/cpulocal.h>
#include <strata/plat/gdt.h>

#include <strata/log.h>
#include <strata/status.h>

#define MODULE_NAME "syscall"

extern void _StSyscallP_Entry(void);

void StSyscallP_Handler(struct StA_InterruptFrame *frame, struct StIntP_Context *ctx)
{
    uint64_t syscall_count = atomic_fetch_add(&StCpuLocalP_GetData()->syscall_count, 1);

    if (syscall_count % 1000000 == 0) {
        LOG_TRACE("syscall count: %" PRIu64 "\n", syscall_count);
    }
}

StStatus StSyscallP_Init(void)
{
    if (!g_p_cpu_features->has_syscall) return STATUS_UNSUPPORTED;

    StA_WriteMsr(MSR_LSTAR, (uintptr_t)_StSyscallP_Entry);
    StA_WriteMsr(
        MSR_STAR,
        ((uint64_t)SEG_SEL_KERNEL_CODE << 32) | (((uint64_t)SEG_SEL_USER_DATA - 8) << 48)
    );
    StA_WriteMsr(MSR_SFMASK, 0x0000000000000202);

    return STATUS_SUCCESS;
}
