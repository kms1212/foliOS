#include "trampoline.h"

#include <stdint.h>

#include <strata/arch/intrinsics/register.h>
#include <strata/arch/mmu.h>

#include <strata/compiler.h>
#include <strata/macros.h>
#include <strata/panic.h>
#include <strata/status.h>
#include <strata/types.h>

static StA_PageMapLevel4Entry st_pml4[512] __aligned(4096);
static StA_PageDirPtrTableEntry st_low_pdpt[512] __aligned(4096);
static StA_PageDirPtrTableEntry st_kernel_pdpt[512] __aligned(4096);
static StA_PaePageDirectoryEntry st_low_pd[512] __aligned(4096);
static StA_PaePageDirectoryEntry st_kernel_pd[512] __aligned(4096);
static StA_PaePageTableEntry st_kernel_pt[16][512] __aligned(4096);
static StA_PageDirPtrTableEntry st_direct_mapping_pdpt[16][512] __aligned(4096);

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
    StA_PageDirectoryEntry *pd = (void *)PAGE_TABLE_RCRS_PD_BASE;
    StA_PageTableEntry *pt = (void *)PAGE_TABLE_RCRS_PT_BASE;
    uint32_t pde_idx;
    uint32_t pt_idx;

    if (vpn > (St_VirtPage)VIRT_PAGE_MAX) return STATUS_INVALID_VALUE;

    pde_idx = (vpn & (St_VirtPage)VIRT_PAGE_PD_INDEX_MASK) >> 10;
    if (!(pd[pde_idx] & STA_MMU_PTE_P)) return STATUS_PAGE_NOT_PRESENT;
    if (pd[pde_idx] & STA_MMU_PTE_PS) {
        *pfn = (((uintptr_t)(pd[pde_idx] >> 22) & 0x3FF) << 10) + (vpn & 0x3FF);

        return STATUS_SUCCESS;
    }

    pt_idx = PAGE_TO_UINT(vpn) & VIRT_PAGE_PT_INDEX_MASK;
    if (!(pt[pt_idx] & STA_MMU_PTE_P)) return STATUS_PAGE_NOT_PRESENT;

    *pfn = (St_PhysFrame)STA_MMU_GET_BASE(pt[pt_idx]);

    return STATUS_SUCCESS;
}

void setup_trampoline_page_tables(void)
{
    /* assume that PD/PT(32p) is recursively mapped at 0xFFC00000-0xFFFFFFFF region */
    StA_PageDirectoryEntry *old_pd = (void *)PAGE_TABLE_RCRS_PD_BASE;
    StA_PageTableEntry *old_pt = (void *)PAGE_TABLE_RCRS_PT_BASE;

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
    st_pml4[0] = STA_MMU_PTE_P | STA_MMU_PTE_RW | STA_MMU_SET_BASE(low_pdpt_pfn);

    /* pml4[511]: 0xFFFFFF80_00000000-0xFFFFFFFF_FFFFFFFF */
    st_pml4[511] = STA_MMU_PTE_P | STA_MMU_PTE_RW | STA_MMU_SET_BASE(kernel_pdpt_pfn);

    status = virt_to_phys(VPTR_TO_PAGE(st_low_pd), &low_pd_pfn);
    if (!CHECK_SUCCESS(status)) goto has_error;

    status = virt_to_phys(VPTR_TO_PAGE(st_kernel_pd), &kernel_pd_pfn);
    if (!CHECK_SUCCESS(status)) goto has_error;

    /* lower direct mapping (temporary) */
    /* pml4[0][0]: 0x00000000_00000000-0x00000000_3FFFFFFF */
    st_low_pdpt[0] = STA_MMU_PTE_P | STA_MMU_PTE_RW | STA_MMU_SET_BASE(low_pd_pfn);

    /* lower kernel mapping (temporary) */
    /* pml4[0][3]: 0x00000000_C0000000-0x00000000_FFFFFFFF */
    st_low_pdpt[3] = STA_MMU_PTE_P | STA_MMU_PTE_RW | STA_MMU_SET_BASE(kernel_pd_pfn);

    /* upper kernel mapping */
    /* pml4[511][510]: 0xFFFFFFFF_80000000-0xFFFFFFFF_BFFFFFFF */
    st_kernel_pdpt[510] = STA_MMU_PTE_P | STA_MMU_PTE_RW | STA_MMU_SET_BASE(kernel_pd_pfn);

    /* identity mapping 0x00000000-0x003FFFFF */
    for (int i = 0; i < 2; i++) {
        st_low_pd[i] = STA_MMU_PTE_P | STA_MMU_PTE_PS | STA_MMU_PTE_RW | STA_MMU_SET_BASE(i);
    }

    /* migrate mappings from 0xC0000000-0xC1FFFFFF (no need to migrate all the kernel area) */
    for (int i = 0; i < ARRAY_SIZE(st_kernel_pt); i++) {
        St_PhysFrame pt_pfn;
        int old_pd_idx = 768 + (i >> 1);

        if (!(old_pd[old_pd_idx] & STA_MMU_PTE_P)) continue;

        status = virt_to_phys(VPTR_TO_PAGE(st_kernel_pt[i]), &pt_pfn);
        if (!CHECK_SUCCESS(status)) goto has_error;

        st_kernel_pd[i] = STA_MMU_PTE_P | (old_pd[old_pd_idx] & STA_MMU_PTE_RW) | STA_MMU_SET_BASE(pt_pfn);
        for (int j = 0; j < 512; j++) {
            int old_pt_idx = (old_pd_idx * 1024) + ((i & 1) * 512) + j;

            if (!(old_pt[old_pt_idx] & STA_MMU_PTE_P)) continue;

            st_kernel_pt[i][j] = STA_MMU_PTE_P | (old_pt[old_pt_idx] & STA_MMU_PTE_RW) | STA_MMU_SET_BASE(STA_MMU_GET_BASE(old_pt[old_pt_idx]));
        }
    }

    /* direct mapping 0xFFFFC000_00000000-0xFFFFC7FF_FFFFFFFF */
    for (int i = 0; i < ARRAY_SIZE(st_direct_mapping_pdpt); i++) {
        St_PhysFrame pdpt_pfn;

        status = virt_to_phys(VPTR_TO_PAGE(st_direct_mapping_pdpt[i]), &pdpt_pfn);
        if (!CHECK_SUCCESS(status)) goto has_error;

        st_pml4[384 + i] = STA_MMU_PTE_P | STA_MMU_PTE_RW | STA_MMU_SET_BASE(pdpt_pfn);

        for (int j = 0; j < ARRAY_SIZE(st_direct_mapping_pdpt[i]); j++) {
            st_direct_mapping_pdpt[i][j] = STA_MMU_PTE_P | STA_MMU_PTE_RW | STA_MMU_PTE_PS | STA_MMU_SET_BASE(i * 512 + j);
        }
    }

    return;

has_error:
    for (;;) {
    }
}

St_PhysFrame get_trampoline_pml4_phys(void)
{
    StStatus status;
    St_PhysFrame pml4_pfn;

    status = virt_to_phys(VPTR_TO_PAGE(st_pml4), &pml4_pfn);
    if (!CHECK_SUCCESS(status)) {
        for (;;) {
        }
    }

    return pml4_pfn;
}
