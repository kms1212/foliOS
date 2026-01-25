#include "trampoline.h"

#include <stdint.h>

#include <strata/arch/intrinsics/register.h>
#include <strata/arch/mmu.h>

#include <strata/panic.h>
#include <strata/status.h>
#include <strata/compiler.h>
#include <strata/types.h>

static union StA_PageMapLevel4Entry st_pml4[512] __aligned(4096);
static union StA_PageDirPtrTableEntry st_low_pdpt[512] __aligned(4096);
static union StA_PageDirPtrTableEntry st_kernel_pdpt[512] __aligned(4096);
static union StA_PaePageDirectoryEntry st_low_pd[512] __aligned(4096);
static union StA_PaePageDirectoryEntry st_kernel_pd[512] __aligned(4096);
static union StA_PaePageTableEntry st_kernel_pt[16][512] __aligned(4096);

#define VIRT_PAGE_MAX           0x000FFFFFUL
#define VIRT_PAGE_PD_MASK       0x000FFC00UL
#define VIRT_PAGE_PD_INDEX_MASK VIRT_PAGE_PD_MASK
#define VIRT_PAGE_PT_MASK       0x000003FFUL
#define VIRT_PAGE_PT_INDEX_MASK (VIRT_PAGE_PD_INDEX_MASK | VIRT_PAGE_PT_MASK)

#define PAGE_TABLE_RCRS_SLOT    1023UL
#define PAGE_TABLE_RCRS_PT_BASE (PAGE_TABLE_RCRS_SLOT << 22)
#define PAGE_TABLE_RCRS_PD_BASE (PAGE_TABLE_RCRS_PT_BASE | (PAGE_TABLE_RCRS_SLOT << 12))

static StStatus virt_to_phys(St_VirtPage vpn __in, St_PhysFrame *pfn __out)
{
    union StA_PageDirectoryEntry *pd = (void *)PAGE_TABLE_RCRS_PD_BASE;
    union StA_PageTableEntry *pt = (void *)PAGE_TABLE_RCRS_PT_BASE;
    uint32_t pde_idx;
    uint32_t pt_idx;

    if (vpn > (St_VirtPage)VIRT_PAGE_MAX) return STATUS_INVALID_VALUE;

    pde_idx = (vpn & (St_VirtPage)VIRT_PAGE_PD_INDEX_MASK) >> 10;
    if (!pd[pde_idx].p) return STATUS_PAGE_NOT_PRESENT;
    if (pd[pde_idx].ps) {
        *pfn = ((uintptr_t)pd[pde_idx].huge.base_low << 10) + (vpn & 0x3FF);

        return STATUS_SUCCESS;
    }

    pt_idx = PAGE_TO_UINT(vpn) & VIRT_PAGE_PT_INDEX_MASK;
    if (!pt[pt_idx].p) return STATUS_PAGE_NOT_PRESENT;

    *pfn = (St_PhysFrame)pt[pt_idx].base;

    return STATUS_SUCCESS;
}

void setup_trampoline_page_tables(void)
{
    /* assume that PD/PT(32p) is recursively mapped at 0xFFC00000-0xFFFFFFFF region */
    union StA_PageDirectoryEntry *old_pd = (void *)PAGE_TABLE_RCRS_PD_BASE;
    union StA_PageTableEntry *old_pt = (void *)PAGE_TABLE_RCRS_PT_BASE;

    StStatus status;
    St_PhysFrame pml4_pfn;
    St_PhysFrame low_pdpt_pfn;
    St_PhysFrame kernel_pdpt_pfn;
    St_PhysFrame low_pd_pfn;
    St_PhysFrame kernel_pd_pfn;
    
    status = virt_to_phys(VPTR_TO_PAGE(st_pml4), &pml4_pfn);
    if (!CHECK_SUCCESS(status)) goto has_error;
    
    status = virt_to_phys(VPTR_TO_PAGE(st_low_pdpt), &low_pdpt_pfn);
    if (!CHECK_SUCCESS(status)) goto has_error;
    
    status = virt_to_phys(VPTR_TO_PAGE(st_kernel_pdpt), &kernel_pdpt_pfn);
    if (!CHECK_SUCCESS(status)) goto has_error;
    
    status = virt_to_phys(VPTR_TO_PAGE(st_kernel_pd), &kernel_pd_pfn);
    if (!CHECK_SUCCESS(status)) goto has_error;

    /*
    |---unavailable---| |--pml4---||---pdpt--||---pd----||----pt---| |page offset-|
    aaaa aaaa bbbb bbbb cccc cccc dddd dddd eeee eeee ffff ffff gggg gggg hhhh hhhh
    */

    /* pml4[0]: 0x00000000_00000000-0x0000007F_FFFFFFFF */
    st_pml4[0].p = 1;
    st_pml4[0].r_w = 1;
    st_pml4[0].base = (uint64_t)low_pdpt_pfn;

    /* pml4[510]: 0xFFFFFF00_00000000-0xFFFFFFFF_7FFFFFFF (recursive mapping) */
    st_pml4[510].p = 1;
    st_pml4[510].r_w = 1;
    st_pml4[510].base = (uint64_t)pml4_pfn;

    /* pml4[511]: 0xFFFFFF80_00000000-0xFFFFFFFF_FFFFFFFF */
    st_pml4[511].p = 1;
    st_pml4[511].r_w = 1;
    st_pml4[511].base = (uint64_t)kernel_pdpt_pfn;

    status = virt_to_phys(VPTR_TO_PAGE(st_low_pd), &low_pd_pfn);
    if (!CHECK_SUCCESS(status)) goto has_error;
    
    status = virt_to_phys(VPTR_TO_PAGE(st_kernel_pd), &kernel_pd_pfn);
    if (!CHECK_SUCCESS(status)) goto has_error;

    /* lower direct mapping (temporary) */
    /* pml4[0][0]: 0x00000000_00000000-0x00000000_3FFFFFFF */
    st_low_pdpt[0].p = 1;
    st_low_pdpt[0].r_w = 1;
    st_low_pdpt[0].base = (uint64_t)low_pd_pfn;

    /* lower kernel mapping (temporary) */
    /* pml4[0][3]: 0x00000000_C0000000-0x00000000_FFFFFFFF */
    st_low_pdpt[3].p = 1;
    st_low_pdpt[3].r_w = 1;
    st_low_pdpt[3].base = (uint64_t)kernel_pd_pfn;

    /* recursive page table mapping */
    /* pml4[511][510]: 0xFFFFFFFF_80000000-0xFFFFFFFF_BFFFFFFF */
    st_kernel_pdpt[510].p = 1;
    st_kernel_pdpt[510].r_w = 1;
    st_kernel_pdpt[510].base = (uint64_t)kernel_pd_pfn;

    /* identity mapping 0x00000000-0x003FFFFF */
    for (int i = 0; i < 2; i++) {
        st_low_pd[i].huge.p = 1;
        st_low_pd[i].huge.ps = 1;
        st_low_pd[i].huge.r_w = 1;
        st_low_pd[i].huge.base = i;
    }

    /* migrate mappings from 0xC0000000-0xC1FFFFFF (no need to migrate all the kernel area) */
    for (int i = 0; i < 16; i++) {
        St_PhysFrame pt_pfn;
        int old_pd_idx = 768 + (i >> 1);

        if (!old_pd[old_pd_idx].p) continue;

        status = virt_to_phys(VPTR_TO_PAGE(st_kernel_pt[i]), &pt_pfn);
        if (!CHECK_SUCCESS(status)) goto has_error;

        st_kernel_pd[i].p = 1;  
        st_kernel_pd[i].r_w = old_pd[old_pd_idx].r_w;
        st_kernel_pd[i].base = (uint64_t)pt_pfn;
        for (int j = 0; j < 512; j++) {
            int old_pt_idx = (old_pd_idx * 1024) + ((i & 1) * 512) + j;
            
            if (!old_pt[old_pt_idx].p) continue;

            st_kernel_pt[i][j].p = 1;
            st_kernel_pt[i][j].r_w = old_pt[old_pt_idx].r_w;
            st_kernel_pt[i][j].base = old_pt[old_pt_idx].base;
        }
    }

    return;

has_error:
    for (;;) {}
}

St_PhysFrame get_trampoline_pml4_phys(void)
{
    StStatus status;
    St_PhysFrame pml4_pfn;
    
    status = virt_to_phys(VPTR_TO_PAGE(st_pml4), &pml4_pfn);
    if (!CHECK_SUCCESS(status)) {
    for (;;) {}
    }

    return pml4_pfn;
}
