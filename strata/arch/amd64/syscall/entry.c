#include <strata/plat/syscall.h>

#include <errno.h>
#include <inttypes.h>
#include <stdatomic.h>
#include <stdint.h>

#include <strata/arch/cpufeatures.h>
#include <strata/arch/interrupt.h>
#include <strata/arch/intrinsics/msr.h>

#include <strata/plat/cpulocal.h>
#include <strata/plat/gdt_constants.h>
#include <strata/plat/interrupt.h>
#include <strata/plat/syscall_num.h>

#include <strata/log.h>
#include <strata/status.h>
#include <strata/syscall.h>
#include <strata/uuid.h>

#define MODULE_NAME "syscall"

extern void _StSyscallA_Handler(void);  // NOLINT

int64_t StSyscallA_Handler(struct StA_InterruptFrame *frame, struct StIntP_Context *ctx)
{
    uint64_t syscall_count = atomic_fetch_add(&StCpuLocalP_GetData()->syscall_count, 1);

    switch (ctx->rax) {
    case SYS_NODE_OPEN: {
        LOG_TRACE(LM_CAT_UNCLASSIFIED, "syscall #%" PRIu64 ": OPEN\n", syscall_count);

        const uint8_t *path = (const uint8_t *)ctx->rdi;  // TODO: use copy_from_user
        uint32_t flags = ctx->rsi;
        uint32_t *handle = (uint32_t *)ctx->rdx;

        return StSyscall_Open(path, flags, handle);
    }
    case SYS_NODE_CLOSE: {
        LOG_TRACE(LM_CAT_UNCLASSIFIED, "syscall #%" PRIu64 ": CLOSE\n", syscall_count);

        uint32_t handle = ctx->rdi;

        return StSyscall_Close(handle);
    }
    case SYS_NODE_QUERY: {
        LOG_TRACE(LM_CAT_UNCLASSIFIED, "syscall #%" PRIu64 ": QUERY\n", syscall_count);

        uint32_t handle = ctx->rdi;
        const struct StUuid *if_uuid = (const struct StUuid *)ctx->rsi;  // TODO: use copy_from_user
        uint32_t request_abiver = ctx->rdx;
        uint32_t *funcid_base = (uint32_t *)ctx->r10;
        uint32_t *result_abiver = (uint32_t *)ctx->r8;

        return StSyscall_Query(handle, if_uuid, request_abiver, funcid_base, result_abiver);
    }
    case SYS_NODE_CALL_REG: {
        LOG_TRACE(LM_CAT_UNCLASSIFIED, "syscall #%" PRIu64 ": CALL_REG\n", syscall_count);

        uint32_t handle = ctx->rdi;
        uint32_t funcid = ctx->rsi;
        unsigned long arg0 = ctx->rdx;
        unsigned long arg1 = ctx->r10;
        unsigned long arg2 = ctx->r8;
        unsigned long arg3 = ctx->r9;

        return StSyscall_CallReg(handle, funcid, arg0, arg1, arg2, arg3);
    }
    case SYS_NODE_CALL_PTR: {
        LOG_TRACE(LM_CAT_UNCLASSIFIED, "syscall #%" PRIu64 ": CALL_PTR\n", syscall_count);

        uint32_t handle = ctx->rdi;
        uint32_t funcid = ctx->rsi;
        const void *args = (const void *)ctx->rdx;  // TODO: use copy_from_user
        void *result = (void *)ctx->r10;            // TODO: use copy_to_user
        unsigned long arg0 = ctx->r8;
        unsigned long arg1 = ctx->r9;

        return StSyscall_CallPtr(handle, funcid, args, result, arg0, arg1);
    }
    default:
        LOG_TRACE(
            LM_CAT_UNCLASSIFIED,
            "syscall #%" PRIu64 ": UNKNOWN %" PRIu64 " (rdi=%#" PRIx64 ", rsi=%#" PRIx64
            ", rdx=%#" PRIx64 ", r10=%#" PRIx64 ", r8=%#" PRIx64 ", r9=%#" PRIx64 ")\n",
            syscall_count,
            ctx->rax,
            ctx->rdi,
            ctx->rsi,
            ctx->rdx,
            ctx->r10,
            ctx->r8,
            ctx->r9
        );
        return -(int64_t)ENOSYS;
    }
}

StStatus StSyscallA_Init(void)
{
    if (!g_p_cpu_features->has_syscall) return STATUS_NOT_SUPPORTED;

    StA_WriteMsr(MSR_LSTAR, (uintptr_t)_StSyscallA_Handler);
    StA_WriteMsr(
        MSR_STAR,
        ((uint64_t)SEG_SEL_KERNEL_CODE << 32) | (((uint64_t)SEG_SEL_USER_DATA - 8) << 48)
    );
    StA_WriteMsr(MSR_SFMASK, 0x0000000000000202);

    return STATUS_SUCCESS;
}
