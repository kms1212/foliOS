#include <strata/plat/thread.h>

#include <stdlib.h>
#include <string.h>

#include <strata/arch/intrinsics/misc.h>
#include <strata/arch/intrinsics/msr.h>
#include <strata/arch/mmu.h>

#include <strata/plat/cpulocal.h>
#include <strata/plat/gdt.h>
#include <strata/plat/tss.h>

#include <strata/compiler.h>
#include <strata/elf.h>
#include <strata/interrupt.h>
#include <strata/log.h>
#include <strata/macros.h>
#include <strata/mm.h>
#include <strata/panic.h>
#include <strata/process.h>
#include <strata/scheduler.h>
#include <strata/thread.h>

#define MODULE_NAME "thread"

extern void _StThreadP_KernelThreadEntry(void);
extern void _StThreadP_UserThreadEntry(void);

StStatus StThreadP_AllocateThreadKernelStack(struct StThread *th __in)
{
    StStatus status;
    St_VirtPage kmode_stack_base_vpn;

    /* allocate thread stack */
    status = StMm_AllocateSparse(
        VMM_DOMAIN_KERNEL_SLOW,
        &kmode_stack_base_vpn,
        th->kmode_stack_page_count,
        PMM_DEFAULT,
        VMM_DEFAULT,
        MAP_DEFAULT
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
        iregs->rbx = (uintptr_t)th->umode_entry;
        iregs->rdi = th->umode_stack_ptr;
    }

    /* align stack pointer */
    rsp -= 8;

    th->kmode_stack_ptr = (void *)rsp;

    return STATUS_SUCCESS;
}

void StThreadP_FreeThreadKernelStack(struct StThread *th __in)
{
    LOG_DEBUG("freeing thread stack...\n");

    StMm_Free(th->kmode_stack_base_vpn, th->kmode_stack_page_count);
}

StStatus StThreadP_AllocateThreadUserStack(struct StThread *th)
{
    StStatus status;
    St_VirtPage ustack_base_vpn = 0x00007FFF80000 - th->umode_stack_page_count;

    /* allocate thread stack */
    status =
        StMm_AllocateSparseTo(ustack_base_vpn, th->umode_stack_page_count, PMM_DEFAULT, MAP_USER);
    if (!CHECK_SUCCESS(status)) return status;

    th->umode_stack_base_vpn = ustack_base_vpn;
    th->umode_stack_ptr = PAGE_TO_ADDR(ustack_base_vpn + th->umode_stack_page_count);

    return STATUS_SUCCESS;
}

static inline void push_u64(uintptr_t *sp __inout, uint64_t val __in)
{
    *sp -= sizeof(uint64_t);

    *(uint64_t *)*sp = val;
}

StStatus StThreadP_SetupThreadUserStack(
    struct StThread *th __in,
    int arg_count,
    const char *const *args,
    int env_count,
    const char *const *envs
)
{
    uintptr_t rsp = th->umode_stack_ptr;
    size_t data_size = 0;
    char *envs_start;
    size_t envs_size = 0;
    char *args_start;
    size_t args_size = 0;
    void *random_start;
    void *auxv_start;

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

    // random data
    data_size = ALIGN(data_size, 16);
    data_size += sizeof(uint8_t) * 16;
    random_start = (void *)(rsp - data_size);

    // auxv
    struct StElf64_Auxv auxv[] = {
        {AT_ENTRY, (uint64_t)th->umode_entry},
        {AT_PAGESZ, 4096},
        {AT_UID, 0},
        {AT_EUID, 0},
        {AT_GID, 0},
        {AT_EGID, 0},
        {AT_RANDOM, (uint64_t)rsp - data_size},
        {AT_NULL, 0},
    };
    data_size += sizeof(auxv);
    auxv_start = (void *)(rsp - data_size);

    rsp -= data_size;
    rsp &= ~0xF;
    if ((3 + arg_count + env_count) & 1) {
        rsp -= sizeof(uint64_t);
    }

    // fill auxv
    memcpy(auxv_start, auxv, sizeof(auxv));

    // fill random data
    uint8_t random_data[16];
    memset(random_data, 0xA5, sizeof(random_data));
    memcpy(random_start, random_data, sizeof(random_data));

    // fill & push envp
    push_u64(&rsp, 0);
    for (int i = env_count - 1; i >= 0; i--) {
        size_t slen = strlen(envs[i]) + 1;
        memcpy((void *)((uintptr_t)envs_start + envs_size - slen), envs[i], slen);
        envs_size -= slen;
        push_u64(&rsp, (uint64_t)envs_start + envs_size);
    }

    // push argv
    push_u64(&rsp, 0);
    for (int i = arg_count - 1; i >= 0; i--) {
        size_t slen = strlen(args[i]) + 1;
        memcpy((void *)((uintptr_t)args_start + args_size - slen), args[i], slen);
        args_size -= slen;
        push_u64(&rsp, (uint64_t)args_start + args_size);
    }

    // push argc
    push_u64(&rsp, arg_count);

    th->umode_stack_ptr = rsp;

    return STATUS_SUCCESS;
}

void StThreadP_FreeThreadUserStack(struct StThread *th __in)
{
    LOG_DEBUG("freeing thread stack...\n");

    StMm_Free(th->umode_stack_base_vpn, th->umode_stack_page_count);
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

__optimize("O0") StStatus StThreadP_Switch(
    struct StThread *next __in, struct StIntP_Context *ctx __in, void **next_stack_ptr __out
)
{
    StStatus status;
    struct StThread *current;
    // St_PhysFrame current_pml4_pfn;
    uintptr_t kstack_top;

    status = StScheduler_GetCurrentThread(&current);
    if (!CHECK_SUCCESS(status)) return status;

    /* save current stack pointer of the previous thread */
    current->kmode_stack_ptr = (void *)((uintptr_t)ctx - 8);

    // current_pml4_pfn = StA_ReadCr3() / PAGE_SIZE;

    // if (next->owner && current_pml4_pfn != next->owner->platform_data.pml4_phys) {
    //     StA_WriteCr3(next->owner->platform_data.pml4_phys * PAGE_SIZE);
    // }

    kstack_top = PAGE_TO_ADDR(next->kmode_stack_base_vpn + next->kmode_stack_page_count);

    StP_SetTssStack(kstack_top);
    StCpuLocalP_GetData()->kernel_rsp = kstack_top;

    if (next->platform_data.fs_base != StA_ReadMsr(MSR_FS_BASE)) {
        StA_WriteMsr(MSR_FS_BASE, next->platform_data.fs_base);
    }

    /* switch to next thread */
    status = StScheduler_SetCurrentThread(next);
    if (!CHECK_SUCCESS(status)) return status;

    LOG_TRACE("task switching: %d -> %d\n", (int)current->id, (int)next->id);

    *next_stack_ptr = next->kmode_stack_ptr;

    return STATUS_SUCCESS;
}

__attribute__((noinline)) __optimize("O0")
    __externally_visible void *_StThreadP_DoYield(struct StIntP_Context *ctx __in)
{
    StStatus status;
    struct StThread *next_thread;
    void *volatile next_stack_ptr;

    if (StThread_IsPreemptionEnabled()) {
        status = StScheduler_GetNextThread(&next_thread);
        if (!CHECK_SUCCESS(status) || !next_thread) return NULL;

        status = StThreadP_Switch(next_thread, ctx, (void **)&next_stack_ptr);
        if (!CHECK_SUCCESS(status)) return NULL;

        return next_stack_ptr;
    }

    return NULL;
}
