#include "strata/plat/cpulocal.h"
#include <strata/plat/thread.h>

#include <stdlib.h>
#include <string.h>

#include <strata/arch/intrinsics/misc.h>
#include <strata/arch/mmu.h>

#include <strata/plat/gdt.h>
#include <strata/plat/tss.h>

#include <strata/process.h>
#include <strata/log.h>
#include <strata/interrupt.h>
#include <strata/panic.h>
#include <strata/scheduler.h>
#include <strata/thread.h>
#include <strata/mm.h>

#define MODULE_NAME "asm_thread"

extern void _StThreadP_RealThreadEntry(void);

StStatus StThreadP_AllocateKThreadStack(struct StThread *th)
{
    StStatus status;
    St_VirtPage kmode_stack_base_vpn;

    /* allocate thread stack */
    status = StMm_AllocateSparse(VMM_DOMAIN_KERNEL_SLOW, &kmode_stack_base_vpn, th->kmode_stack_page_count, PMM_DEFAULT, VMM_DEFAULT, MAP_DEFAULT);
    if (!CHECK_SUCCESS(status)) return status;

    th->kmode_stack_base_vpn = kmode_stack_base_vpn * PAGE_SIZE;
    th->kmode_stack_ptr = (void *)((kmode_stack_base_vpn + th->kmode_stack_page_count) * PAGE_SIZE);

    return STATUS_SUCCESS;
}

StStatus StThreadP_SetupKThreadStack(struct StThread *th)
{
    uintptr_t rsp;
    struct StIntP_Context *iregs;
    struct StA_InterruptFrame *iframe;

    rsp = (uintptr_t)th->kmode_stack_ptr;

    /* fill initial interrupt stack frame */
    rsp -= sizeof(*iframe);
    iframe = (void *)rsp;
    memset(iframe, 0, sizeof(*iframe));

    if (th->type == THREAD_TYPE_KERNEL) {
        iframe->ss = SEG_SEL_KERNEL_DATA;
        iframe->rsp = (uintptr_t)th->kmode_stack_ptr;
        iframe->rflags = 0x0000000000000202;
        iframe->cs = SEG_SEL_KERNEL_CODE;
        iframe->rip = (uintptr_t)_StThreadP_RealThreadEntry;
    } else {
        iframe->ss = SEG_SEL_USER_DATA | 3;
        iframe->rsp = th->platform_data.user_rsp;
        iframe->rflags = 0x0000000000000202;
        iframe->cs = SEG_SEL_USER_CODE | 3;
        iframe->rip = th->umode_entry;
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
    }

    th->kmode_stack_ptr = (void *)rsp;

    return STATUS_SUCCESS;
}

void StThreadP_FreeKThreadStack(struct StThread *th)
{
    LOG_DEBUG("freeing thread stack...\n");

    StMm_Free(th->kmode_stack_base_vpn, th->kmode_stack_page_count);
}

StStatus StThreadP_Switch(struct StThread *next, struct StIntP_Context *ctx, void **next_stack_ptr)
{
    StStatus status;
    struct StThread *current;
    St_PhysFrame current_pml4_pfn;
    uintptr_t kstack_top;

    status = StScheduler_GetCurrentThread(&current);
    if (!CHECK_SUCCESS(status)) return status;

    /* check thread status */
    switch (next->status) {
        case THREAD_STATE_PENDING:
            next->status = THREAD_STATE_RUNNING;
            break;
        case THREAD_STATE_RUNNING:
            break;
        case THREAD_STATE_BLOCKING:
            break;
        case THREAD_STATE_FINISHED:
            break;
        default:
            St_Panic(STATUS_SYSTEM_CORRUPTED, "system corrupted");
    }
    
    /* save current stack pointer of the previous thread */
    current->kmode_stack_ptr = ctx;

    current_pml4_pfn = StA_ReadCr3() / PAGE_SIZE;

    if (next->owner && current_pml4_pfn != next->owner->platform_data.pml4_phys) {
        StA_WriteCr3(next->owner->platform_data.pml4_phys * PAGE_SIZE);
    }

    kstack_top = (next->kmode_stack_base_vpn + next->kmode_stack_page_count) * PAGE_SIZE;

    StP_SetTssStack(kstack_top);
    StCpuLocalP_GetData()->kernel_rsp = kstack_top;

    /* switch to next thread */
    status = StScheduler_SetCurrentThread(next);
    if (!CHECK_SUCCESS(status)) return status;

    if (next_stack_ptr) *next_stack_ptr = next->kmode_stack_ptr;

    return STATUS_SUCCESS;
}

