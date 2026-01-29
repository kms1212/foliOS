#include "strata/compiler.h"
#include <strata/plat/thread.h>

#include <stdlib.h>
#include <string.h>

#include <strata/arch/intrinsics/misc.h>
#include <strata/arch/mmu.h>

#include <strata/plat/cpulocal.h>
#include <strata/plat/gdt.h>
#include <strata/plat/tss.h>

#include <strata/interrupt.h>
#include <strata/log.h>
#include <strata/mm.h>
#include <strata/panic.h>
#include <strata/process.h>
#include <strata/scheduler.h>
#include <strata/thread.h>

#define MODULE_NAME "thread"

extern void _StThreadP_KernelThreadEntry(void);
extern void _StThreadP_UserThreadEntry(void);

StStatus StThreadP_AllocateKThreadStack(struct StThread *th __in)
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

StStatus StThreadP_SetupKThreadStack(struct StThread *th __in)
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

void StThreadP_FreeKThreadStack(struct StThread *th __in)
{
    LOG_DEBUG("freeing thread stack...\n");

    StMm_Free(th->kmode_stack_base_vpn, th->kmode_stack_page_count);
}

StStatus StThreadP_Switch(
    struct StThread *next __in, struct StIntP_Context *ctx __in, void **next_stack_ptr __out
)
{
    StStatus status;
    struct StThread *current;
    St_PhysFrame current_pml4_pfn;
    uintptr_t kstack_top;

    status = StScheduler_GetCurrentThread(&current);
    if (!CHECK_SUCCESS(status)) return status;

    /* save current stack pointer of the previous thread */
    current->kmode_stack_ptr = (void *)((uintptr_t)ctx - 8);

    current_pml4_pfn = StA_ReadCr3() / PAGE_SIZE;

    // if (next->owner && current_pml4_pfn != next->owner->platform_data.pml4_phys) {
    //     StA_WriteCr3(next->owner->platform_data.pml4_phys * PAGE_SIZE);
    // }

    kstack_top = PAGE_TO_ADDR(next->kmode_stack_base_vpn + next->kmode_stack_page_count);

    StP_SetTssStack(kstack_top);
    StCpuLocalP_GetData()->kernel_rsp = kstack_top;

    /* switch to next thread */
    status = StScheduler_SetCurrentThread(next);
    if (!CHECK_SUCCESS(status)) return status;

    LOG_TRACE("task switching: %d -> %d\n", (int)current->id, (int)next->id);

    *next_stack_ptr = next->kmode_stack_ptr;

    return STATUS_SUCCESS;
}

/*
void StThreadP_Yield(void)
{
    __asm__ volatile (
        "pushfq\n\t"
        "cli\n\t"
        "sub $8, %%rsp\n\t"
        "int $0x20\n\t"
        "add $8, %%rsp\n\t"
        "popfq\n\t"
        : : : "memory"
    );
}
    */

__externally_visible void *_StThreadP_DoYield(struct StIntP_Context *ctx __in)
{
    StStatus status;
    struct StThread *next_thread;
    void *next_stack_ptr;

    if (StThread_IsPreemptionEnabled()) {
        status = StScheduler_GetNextThread(&next_thread);
        if (!CHECK_SUCCESS(status) || !next_thread) return NULL;

        status = StThreadP_Switch(next_thread, ctx, &next_stack_ptr);
        if (!CHECK_SUCCESS(status)) return NULL;

        return next_stack_ptr;
    }

    return NULL;
}
