#include <strata/plat/syscall.h>

#include <inttypes.h>
#include <time.h>

#include <strata/arch/cpufeatures.h>
#include <strata/arch/interrupt.h>
#include <strata/arch/intrinsics/msr.h>

#include <strata/plat/cpulocal.h>
#include <strata/plat/gdt.h>
#include <strata/plat/thread.h>
#include <strata/plat/time.h>

#include <strata/log.h>
#include <strata/status.h>

#define MODULE_NAME "syscall"

extern void _StSyscallP_Entry(void);

struct iovec {
    void *iov_base;
    size_t iov_len;
};

long StSyscallP_Handler(struct StA_InterruptFrame *frame, struct StIntP_Context *ctx)
{
    uint64_t syscall_count = atomic_fetch_add(&StCpuLocalP_GetData()->syscall_count, 1);
    struct StThread *current = StCpuLocalP_GetData()->scheduler.current_thread;

    LOG_DEBUG("syscall #%" PRIu64 ": number %" PRIu64 "\n", syscall_count, ctx->rax);

    switch (ctx->rax) {
    case 20: {  // writev
        struct iovec *iov = (struct iovec *)ctx->rsi;
        size_t iovcnt = ctx->rdx;
        size_t io_count = 0;
        for (size_t i = 0; i < iovcnt; i++) {
            LOG_DEBUG(
                "writev: %*s -> %" PRIu64 "\n",
                (int)iov[i].iov_len,
                (char *)iov[i].iov_base,
                ctx->rdi
            );
            io_count += iov[i].iov_len;
        }
        return io_count;
    }
    case 35: {  // nanosleep
        struct timespec *req = (struct timespec *)ctx->rdi;
        struct timespec *rem = (struct timespec *)ctx->rsi;
        uint64_t start = StTimeP_GetUptimeMicroseconds();

        StThread_Sleep(req->tv_sec * 1000 + req->tv_nsec / 1000000);

        uint64_t elapsed = StTimeP_GetUptimeMicroseconds() - start;
        if (rem) {
            rem->tv_sec = elapsed / 1000000;
            rem->tv_nsec = (elapsed % 1000000) * 1000;
        }

        return 0;
    }
    case 60:  // exit
        StThread_Exit();
        return -1;
    case 158:  // arch_prctl
        switch (ctx->rdi) {
        case 0x1001:  // ARCH_SET_GS
            StThreadP_SetGsBase(current, ctx->rsi);
            return 0;
        case 0x1002:  // ARCH_SET_FS
            StThreadP_SetFsBase(current, ctx->rsi);
            return 0;
        case 0x1003:  // ARCH_GET_FS
            *(uint64_t *)ctx->rsi = current->platform_data.fs_base;
            return 0;
        case 0x1004:  // ARCH_GET_GS
            *(uint64_t *)ctx->rsi = current->platform_data.gs_base;
            return 0;
        default:
            return -1;
        }
    default:
        return -1;
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
