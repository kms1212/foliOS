#include <strata/plat/syscall.h>

#include <inttypes.h>

#include <strata/arch/cpufeatures.h>
#include <strata/arch/interrupt.h>
#include <strata/arch/intrinsics/msr.h>

#include <strata/plat/cpulocal.h>
#include <strata/plat/gdt.h>
#include <strata/plat/thread.h>
#include <strata/plat/time.h>

#include <strata/log.h>
#include <strata/status.h>
#include <strata/syscall.h>

#define MODULE_NAME "syscall"

extern void _StSyscallP_Entry(void);

StStatus StSyscallP_Handler(struct StA_InterruptFrame *frame, struct StIntP_Context *ctx)
{
    uint64_t syscall_count = atomic_fetch_add(&StCpuLocalP_GetData()->syscall_count, 1);
    struct StThread *current = StCpuLocalP_GetData()->scheduler.current_thread;
    static uint32_t new_handle_id = 0;

    LOG_DEBUG(
        LM_CAT_UNCLASSIFIED,
        "syscall #%" PRIu64 ": number %" PRIu64 "\n",
        syscall_count,
        ctx->rax
    );

    switch (ctx->rax) {
    case SYS_NODE_OPEN: {
        const uint8_t *path = (const uint8_t *)ctx->rdi;  // TODO: use copy_from_user
        uint32_t flags = ctx->rsi;
        uint32_t *handle = (uint32_t *)ctx->rdx;

        return StSyscall_Open(path, flags, handle);
    }
    case SYS_NODE_CLOSE: {
        uint32_t handle = ctx->rdi;

        return StSyscall_Close(handle);
    }
    case SYS_NODE_QUERY: {
        uint32_t handle = ctx->rdi;
        const struct StUuid *if_uuid = (const struct StUuid *)ctx->rsi;  // TODO: use copy_from_user
        uint32_t request_groupid = ctx->rdx;
        uint32_t request_abiver = ctx->r8;
        uint32_t *funcid_base = (uint32_t *)ctx->r9;
        uint32_t *result_abiver = (uint32_t *)ctx->r10;

        return StSyscall_Query(
            handle,
            if_uuid,
            request_groupid,
            request_abiver,
            funcid_base,
            result_abiver
        );
    }
    case SYS_NODE_CALL_REG: {
        return STATUS_NOT_IMPLEMENTED;
    }
    case SYS_NODE_CALL_PTR: {
        return STATUS_NOT_IMPLEMENTED;
    }
    default:
        return STATUS_NOT_IMPLEMENTED;
    }
}

StStatus StSyscallP_Init(void)
{
    if (!g_p_cpu_features->has_syscall) return STATUS_NOT_SUPPORTED;

    StA_WriteMsr(MSR_LSTAR, (uintptr_t)_StSyscallP_Entry);
    StA_WriteMsr(
        MSR_STAR,
        ((uint64_t)SEG_SEL_KERNEL_CODE << 32) | (((uint64_t)SEG_SEL_USER_DATA - 8) << 48)
    );
    StA_WriteMsr(MSR_SFMASK, 0x0000000000000202);

    return STATUS_SUCCESS;
}
