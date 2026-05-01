#include <strata/plat/thread.h>

#include "config.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <strata/arch/interrupt.h>
#include <strata/arch/intrinsics/fpu_simd.h>
#include <strata/arch/intrinsics/msr.h>
#include <strata/arch/mmu.h>

#include <strata/plat/cpulocal.h>
#include <strata/plat/gdt_constants.h>
#include <strata/plat/memmap.h>
#include <strata/plat/mm.h>
#include <strata/plat/tss.h>

#include <strata/compiler.h>
#include <strata/arch/cpufeatures.h>
#include <strata/elf.h>
#include <strata/interrupt.h>
#include <strata/log.h>
#include <strata/macros.h>
#include <strata/mm.h>
#include <strata/mm/pmm.h>
#include <strata/mm/types.h>
#include <strata/mm/utils.h>
#include <strata/mm/vmm.h>
#include <strata/panic.h>
#include <strata/process.h>
#include <strata/scheduler.h>
#include <strata/status.h>
#include <strata/thread.h>

#define MODULE_NAME                          "thread"
#define THREAD_KERNEL_STACK_CACHE_MAX_STACKS ((St_PageCount)8)
#define THREAD_KERNEL_STACK_CACHE_MAX_PAGES                                                        \
    (STRATA_KSTACK_PAGE_COUNT * THREAD_KERNEL_STACK_CACHE_MAX_STACKS)
#define THREAD_KERNEL_STACK_CACHE_LOW_FREE_WATERMARK ((St_PageCount)2048)

extern void _StThreadP_KernelThreadEntry(void);
extern void _StThreadP_UserThreadEntry(void);
extern struct StMm_AddressSpace base_asp;

struct cached_kernel_stack {
    struct cached_kernel_stack *next;
};

static struct cached_kernel_stack *kernel_stack_cache_head = NULL;
static St_PageCount kernel_stack_cache_pages = 0;
static struct StThreadP_PlatformData clean_platform_data;
static uint64_t xstate_mask = 0;
static int xsave_context_enabled = 0;
static int avx_context_enabled = 0;
static int clean_fx_state_initialized = 0;

static void save_fpu_simd_state(struct StThreadP_PlatformData *platform_data)
{
    if (xsave_context_enabled) {
        StA_XSave(&platform_data->xstate_buffer, xstate_mask);
    } else {
        StA_FXSave(&platform_data->xstate_buffer.fx);
    }
}

static void restore_fpu_simd_state(const struct StThreadP_PlatformData *platform_data)
{
    if (xsave_context_enabled) {
        StA_XRestore(&platform_data->xstate_buffer, xstate_mask);
    } else {
        StA_FXRestore((union StA_FXSaveBuffer *)&platform_data->xstate_buffer.fx);
    }
}

StStatus StThreadP_InitializeFpuSimdState(void)
{
    struct StThreadP_PlatformData saved_platform_data;

    if (clean_fx_state_initialized) return STATUS_ALREADY_PERFORMED;

    xsave_context_enabled = g_p_cpu_features->has_xsave;
    avx_context_enabled = xsave_context_enabled && g_p_cpu_features->has_avx;
    xstate_mask = xsave_context_enabled ? (avx_context_enabled ? 0x7ULL : 0x3ULL) : 0;

    memset(&saved_platform_data, 0, sizeof(saved_platform_data));
    save_fpu_simd_state(&saved_platform_data);
    StA_FNInit();
    StA_LdMxcsr(0x1F80);
    if (avx_context_enabled) {
        StA_VZeroAll();
    } else {
        StA_ZeroXmmRegisters();
    }

    memset(&clean_platform_data, 0, sizeof(clean_platform_data));
    save_fpu_simd_state(&clean_platform_data);
    restore_fpu_simd_state(&saved_platform_data);

    clean_fx_state_initialized = 1;

    return STATUS_SUCCESS;
}

void StThreadP_InitializePlatformData(struct StThread *th)
{
    if (!th) return;

    if (!clean_fx_state_initialized) {
        St_Panic(
            STATUS_CONFLICTING_STATE,
            "clean FPU/SIMD state was not initialized before thread creation"
        );
    }

    memcpy(&th->platform_data, &clean_platform_data, sizeof(th->platform_data));
}

static int should_use_kernel_stack_cache(const struct StThread *th)
{
    return th && th->kmode_stack_page_count == STRATA_KSTACK_PAGE_COUNT;
}

St_PageCount StThreadP_ReclaimCachedKernelStacks(St_PageCount page_budget)
{
    St_PageCount reclaimed_pages = 0;

    while (page_budget == 0 || reclaimed_pages < page_budget) {
        struct cached_kernel_stack *cached_stack;
        St_VirtPage base_vpn;

        StThread_LockPreemption();

        cached_stack = kernel_stack_cache_head;
        if (cached_stack) {
            kernel_stack_cache_head = cached_stack->next;
            kernel_stack_cache_pages -= STRATA_KSTACK_PAGE_COUNT;
        }

        StThread_UnlockPreemption();

        if (!cached_stack) break;

        base_vpn = ADDR_TO_PAGE((uintptr_t)cached_stack);
        StMm_FreeGlobal(VMM_DOMAIN_KERNEL_SLOW, base_vpn, STRATA_KSTACK_PAGE_COUNT);

        reclaimed_pages += STRATA_KSTACK_PAGE_COUNT;
    }

    return reclaimed_pages;
}

StStatus StThreadP_AllocateThreadKernelStack(struct StThread *th __in)
{
    StStatus status;
    St_VirtPage kmode_stack_base_vpn;
    struct cached_kernel_stack *cached_stack;

    if (should_use_kernel_stack_cache(th)) {
        StThread_LockPreemption();

        cached_stack = kernel_stack_cache_head;
        if (cached_stack) {
            kernel_stack_cache_head = cached_stack->next;
            kernel_stack_cache_pages -= th->kmode_stack_page_count;
        }

        StThread_UnlockPreemption();

        if (cached_stack) {
            kmode_stack_base_vpn = ADDR_TO_PAGE((uintptr_t)cached_stack);
            th->kmode_stack_base_vpn = kmode_stack_base_vpn;
            th->kmode_stack_ptr = PAGE_TO_VPTR(kmode_stack_base_vpn + th->kmode_stack_page_count);
            return STATUS_SUCCESS;
        }
    }

    /* allocate thread stack */
    status = StMm_AllocateGlobalSparse(
        VMM_DOMAIN_KERNEL_SLOW,
        &kmode_stack_base_vpn,
        th->kmode_stack_page_count,
        NULL,
        AF_DEFAULT,
        MF_KERNEL_DEFAULT
    );
    if (!CHECK_SUCCESS(status)) return status;

    th->kmode_stack_base_vpn = kmode_stack_base_vpn;
    th->kmode_stack_ptr = PAGE_TO_VPTR(kmode_stack_base_vpn + th->kmode_stack_page_count);

    return STATUS_SUCCESS;
}

StStatus StThreadP_SetupThreadKernelStack(struct StThread *th __in)
{
    uintptr_t rsp;
    struct StIntP_Context *iregs;
    struct StA_InterruptFrame *iframe;

    rsp = (uintptr_t)th->kmode_stack_ptr;

    /* fill initial interrupt stack frame */
    rsp -= sizeof(*iframe);
    iframe = (void *)rsp;
    memset(iframe, 0, sizeof(*iframe));

    iframe->ss = SEG_SEL_KERNEL_DATA;
    iframe->rflags = 0x0000000000000202;
    iframe->cs = SEG_SEL_KERNEL_CODE;
    iframe->rsp = (uintptr_t)th->kmode_stack_ptr;

    if (th->type == THREAD_TYPE_KERNEL) {
        iframe->rip = (uintptr_t)_StThreadP_KernelThreadEntry;
    } else {
        iframe->rip = (uintptr_t)_StThreadP_UserThreadEntry;
    }

    /* fill initial register stack */
    rsp -= sizeof(*iregs);
    iregs = (void *)rsp;
    memset(iregs, 0, sizeof(*iregs));

    if (th->type == THREAD_TYPE_KERNEL) {
        iregs->rbx = (uintptr_t)th->kmode_entry;
        iregs->rdi = (uintptr_t)th;
    } else {
        iregs->rbx = th->umode_entry;
        iregs->rdi = th->umode_stack_ptr;
    }

    /* align stack pointer */
    rsp -= 8;

    th->kmode_stack_ptr = (void *)rsp;

    return STATUS_SUCCESS;
}

void StThreadP_FreeThreadKernelStack(struct StThread *th __in)
{
    StStatus status;
    St_PageCount free_frames = 0;

    if (should_use_kernel_stack_cache(th)) {
        status = StPmm_GetFreeFrameCount(&free_frames);
        if (CHECK_SUCCESS(status) && free_frames > THREAD_KERNEL_STACK_CACHE_LOW_FREE_WATERMARK) {
            StThread_LockPreemption();

            if (kernel_stack_cache_pages + th->kmode_stack_page_count <=
                THREAD_KERNEL_STACK_CACHE_MAX_PAGES) {
                struct cached_kernel_stack *cached_stack =
                    (struct cached_kernel_stack *)PAGE_TO_VPTR(th->kmode_stack_base_vpn);

                cached_stack->next = kernel_stack_cache_head;
                kernel_stack_cache_head = cached_stack;
                kernel_stack_cache_pages += th->kmode_stack_page_count;

                StThread_UnlockPreemption();
                return;
            }

            StThread_UnlockPreemption();
        }
    }

    LOG_DEBUG(LM_CAT_UNCLASSIFIED, "freeing thread kernel stack...\n");

    StMm_FreeGlobal(VMM_DOMAIN_KERNEL_SLOW, th->kmode_stack_base_vpn, th->kmode_stack_page_count);
}

StStatus StThreadP_AllocateThreadUserStack(struct StThread *th)
{
    StStatus status;
    St_VirtPage ustack_base_vpn = MEMMAP_USER_VPN_LIMIT + 1 - th->umode_stack_page_count;

    /* allocate thread stack */
    status = StMm_AllocateLocalSparseTo(
        th->process->address_space,
        ustack_base_vpn,
        th->umode_stack_page_count,
        AF_DEFAULT,
        MF_USER_DEFAULT
    );
    if (!CHECK_SUCCESS(status)) return status;

    th->umode_stack_base_vpn = ustack_base_vpn;
    th->umode_stack_ptr = PAGE_TO_ADDR(ustack_base_vpn + th->umode_stack_page_count);

    return STATUS_SUCCESS;
}

static inline void push_u64(
    struct StMm_AddressSpace *asp __in, uintptr_t *sp __inout, uint64_t val __in
)
{
    *sp -= sizeof(uint64_t);

    StMm_WriteLocal(asp, *sp, &val, sizeof(val));
}

StStatus StThreadP_SetupThreadUserStack(
    struct StThread *th __in,
    int arg_count,
    const char *const *args,
    int env_count,
    const char *const *envs
)
{
    StStatus status;
    struct StMm_AddressSpace *asp = th->process->address_space;
    uintptr_t rsp = th->umode_stack_ptr;
    size_t data_size = 0;
    char *envs_start;
    size_t envs_size = 0;
    char *args_start;
    size_t args_size = 0;
    void *random_start;
    void *execfn_start;

    // envs
    for (int i = 0; i < env_count; i++) {
        envs_size += strlen(envs[i]) + 1;
    }
    data_size += envs_size;
    envs_start = (void *)(rsp - data_size);

    // args
    for (int i = 0; i < arg_count; i++) {
        args_size += strlen(args[i]) + 1;
    }
    data_size += args_size;
    args_start = (void *)(rsp - data_size);

    // execfn
    data_size += strlen(args[0]) + 1;
    execfn_start = (void *)(rsp - data_size);

    // random data
    data_size = ALIGN(data_size, 16);
    data_size += sizeof(uint8_t) * 16;
    random_start = (void *)(rsp - data_size);

    // auxv
    struct StElf64_Auxv auxv[] = {
        {AT_ENTRY, th->umode_entry},
        {AT_PAGESZ, PAGE_SIZE},
        {AT_UID, 0},
        {AT_EUID, 0},
        {AT_GID, 0},
        {AT_EGID, 0},
        {AT_RANDOM, (uintptr_t)random_start},
        {AT_EXECFN, (uintptr_t)execfn_start},
        {AT_SYSINFO, (uintptr_t)0xFFFF800000000000},
        {AT_NULL, 0},
    };
    data_size += sizeof(auxv);

    // make stack space and align stack pointer
    rsp -= data_size;
    rsp &= ~0xF;
    if ((3 + arg_count + env_count) & 1) {
        rsp -= sizeof(uint64_t);
    }

    // fill execfn
    status = StMm_WriteLocal(asp, (uintptr_t)execfn_start, args[0], strlen(args[0]) + 1);
    if (!CHECK_SUCCESS(status)) goto has_error;

    // fill random data
    uint8_t random_data[16];
    memset(random_data, 0xA5, sizeof(random_data));
    status = StMm_WriteLocal(asp, (uintptr_t)random_start, random_data, sizeof(random_data));
    if (!CHECK_SUCCESS(status)) goto has_error;

    // fill auxv
    status = StMm_WriteLocal(asp, rsp, auxv, sizeof(auxv));
    if (!CHECK_SUCCESS(status)) goto has_error;

    // fill & push envp
    push_u64(asp, &rsp, 0);
    for (int i = env_count - 1; i >= 0; i--) {
        size_t slen = strlen(envs[i]) + 1;
        status = StMm_WriteLocal(asp, (uintptr_t)envs_start + envs_size - slen, envs[i], slen);
        if (!CHECK_SUCCESS(status)) goto has_error;
        envs_size -= slen;
        push_u64(asp, &rsp, (uint64_t)envs_start + envs_size);
    }

    // push argv
    push_u64(asp, &rsp, 0);
    for (int i = arg_count - 1; i >= 0; i--) {
        size_t slen = strlen(args[i]) + 1;
        status = StMm_WriteLocal(asp, (uintptr_t)args_start + args_size - slen, args[i], slen);
        if (!CHECK_SUCCESS(status)) goto has_error;
        args_size -= slen;
        push_u64(asp, &rsp, (uint64_t)args_start + args_size);
    }

    // push argc
    push_u64(asp, &rsp, arg_count);

    th->umode_stack_ptr = rsp;

    return STATUS_SUCCESS;

has_error:
    return status;
}

void StThreadP_FreeThreadUserStack(struct StThread *th __in)
{
    LOG_DEBUG(LM_CAT_UNCLASSIFIED, "freeing thread user stack...\n");

    StMm_FreeLocal(
        th->process->address_space,
        th->umode_stack_base_vpn,
        th->umode_stack_page_count
    );
}

StStatus StThreadP_SetFsBase(struct StThread *th __in, uintptr_t fs_base __in)
{
    StStatus status;
    struct StThread *current;

    th->platform_data.fs_base = fs_base;

    status = StScheduler_GetCurrentThread(&current);
    if (!CHECK_SUCCESS(status)) return status;

    if (th == current) {
        StA_WriteMsr(MSR_FS_BASE, fs_base);
    }

    return STATUS_SUCCESS;
}

StStatus StThreadP_SetGsBase(struct StThread *th __in, uintptr_t gs_base __in)
{
    StStatus status;
    struct StThread *current;

    th->platform_data.gs_base = gs_base;

    status = StScheduler_GetCurrentThread(&current);
    if (!CHECK_SUCCESS(status)) return status;

    if (th == current) {
        StA_WriteMsr(MSR_KERNEL_GS_BASE, gs_base);
    }

    return STATUS_SUCCESS;
}

StStatus StThreadP_Switch(
    struct StThread *next __in, struct StIntP_Context *ctx __in, void **next_stack_ptr __out
)
{
    StStatus status;
    struct StThread *current;
    struct StMm_AddressSpace *current_asp = StCpuLocalP_GetData()->current_asp;
    uintptr_t kstack_top;

    status = StScheduler_GetCurrentThread(&current);
    if (!CHECK_SUCCESS(status)) return status;

    /* save current stack pointer of the previous thread */
    current->kmode_stack_ptr = (void *)((uintptr_t)ctx - 8);

    /* save FPU/SIMD registers */
    save_fpu_simd_state(&current->platform_data);

    /* switch address space */
    if (next->type == THREAD_TYPE_USER) {
        status = StMmP_SwitchAddressSpace(next->process->address_space);
        if (!CHECK_SUCCESS(status)) return status;
    } else if (current_asp != &base_asp) {
        status = StMmP_SwitchAddressSpace(&base_asp);
        if (!CHECK_SUCCESS(status)) return status;
    }

    /* restore FPU/SIMD registers */
    restore_fpu_simd_state(&next->platform_data);

    /* set kernel stack pointer */
    kstack_top = PAGE_TO_ADDR(next->kmode_stack_base_vpn + next->kmode_stack_page_count);
    StP_SetTssStack(kstack_top);
    StCpuLocalP_GetData()->kernel_rsp = kstack_top;

    /* set FS base */
    if (next->platform_data.fs_base != current->platform_data.fs_base) {
        StA_WriteMsr(MSR_FS_BASE, next->platform_data.fs_base);
    }

    /* set GS base */
    if (next->platform_data.gs_base != current->platform_data.gs_base) {
        StA_WriteMsr(MSR_KERNEL_GS_BASE, next->platform_data.gs_base);
    }

    /* switch to next thread */
    status = StScheduler_SwitchCurrentThread(next);
    if (!CHECK_SUCCESS(status)) return status;

    atomic_fetch_add(&StCpuLocalP_GetData()->ctxswitch_count, 1);

    LOG_TRACE(
        LM_CAT_THREAD | LM_SUBCAT_TASK_SWITCH,
        "task switching: %d -> %d\n",
        (int)current->id,
        (int)next->id
    );

    *next_stack_ptr = next->kmode_stack_ptr;

    return STATUS_SUCCESS;
}

__attribute__((noinline)) __externally_visible void *_StThreadP_DoYield(
    struct StIntP_Context *ctx __in
)
{
    StStatus status;
    struct StThread *current_thread;
    struct StThread *next_thread;
    void *volatile next_stack_ptr;

    if (StThread_IsPreemptionEnabled()) {
        if (StScheduler_ShouldMaintain()) {
            status = StScheduler_Maintain();
            if (!CHECK_SUCCESS(status)) return NULL;
        }

        status = StScheduler_GetNextThread(&next_thread);
        if (!CHECK_SUCCESS(status) || !next_thread) return NULL;

        status = StScheduler_GetCurrentThread(&current_thread);
        if (!CHECK_SUCCESS(status) || !current_thread) return NULL;

        if (next_thread == current_thread) {
            return NULL;
        }

        status = StThreadP_Switch(next_thread, ctx, (void **)&next_stack_ptr);
        if (!CHECK_SUCCESS(status)) return NULL;

        return next_stack_ptr;
    }

    return NULL;
}
