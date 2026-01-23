#include <strata/plat/thread.h>

#include <stdlib.h>
#include <string.h>

#include <strata/arch/intrinsics/misc.h>
#include <strata/arch/mmu.h>

#include <strata/plat/gdt.h>

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
    St_VirtPage kmode_stack_base_vpn = 0;

    /* allocate thread stack */
    status = StMm_AllocateSparse(VMM_DOMAIN_KERNEL_FAST, &kmode_stack_base_vpn, th->kmode_stack_page_count, PMM_DEFAULT, VMM_DEFAULT, MAP_DEFAULT);
    if (!CHECK_SUCCESS(status)) return status;

    th->kmode_stack_base_vpn = kmode_stack_base_vpn * PAGE_SIZE;
    th->kmode_stack_ptr = (void *)((kmode_stack_base_vpn + th->kmode_stack_page_count) * PAGE_SIZE);

    return STATUS_SUCCESS;
}

StStatus StThreadP_SetupKThreadStack(struct StThread *th)
{
    uintptr_t esp;
    struct StIntP_Context *iregs;
    struct StA_InterruptFrame *iframe;

    esp = (uintptr_t)th->kmode_stack_ptr;

    /* fill initial interrupt stack frame */
    esp -= sizeof(*iframe);
    iframe = (void *)esp;
    memset(iframe, 0, sizeof(*iframe));
    iframe->eflags = 0x00000202;
    iframe->cs = SEG_SEL_KERNEL_CODE;
    iframe->eip = (uintptr_t)_StThreadP_RealThreadEntry;

    /* fill initial register stack */
    esp -= sizeof(*iregs);
    iregs = (void *)esp;
    memset(iregs, 0, sizeof(*iregs));
    iregs->pushal.ebx = (uintptr_t)th;
    iregs->pushal.ecx = (uintptr_t)th->kmode_entry;
    iregs->ds = SEG_SEL_KERNEL_DATA;
    iregs->es = SEG_SEL_KERNEL_DATA;
    iregs->fs = SEG_SEL_KERNEL_DATA;
    iregs->gs = SEG_SEL_KERNEL_DATA;

    /* stack area for dummy ebp */
    esp -= 4;

    th->kmode_stack_ptr = (void *)esp;

    return STATUS_SUCCESS;
}

void StThreadP_FreeKThreadStack(struct StThread *th)
{
    StStatus status;
    St_PhysFrame kmode_stack_base_pfn;

    LOG_DEBUG("freeing thread stack...\n");

    status = StMm_VirtPageToPhysFrame(th->kmode_stack_base_vpn, &kmode_stack_base_pfn);
    if (!CHECK_SUCCESS(status)) {
        St_Panic(status, "failed to get kmode_stack_base page mapping");
    }

    status = StMm_Unmap(th->kmode_stack_base_vpn, th->kmode_stack_page_count);
    if (!CHECK_SUCCESS(status)) {
        St_Panic(status, "failed to unmap kmode_stack_base");
    }

    StVmm_FreePage(th->kmode_stack_base_vpn, th->kmode_stack_page_count);
    StPmm_FreeFrame(kmode_stack_base_pfn, th->kmode_stack_page_count);
}
