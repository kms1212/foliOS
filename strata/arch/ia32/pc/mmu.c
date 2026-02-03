#include <strata/plat/mm.h>

#include <strata/arch/mmu.h>

#define VIRT_PAGE_MAX           0x000FFFFFUL
#define VIRT_PAGE_PD_MASK       0x000FFC00UL
#define VIRT_PAGE_PD_INDEX_MASK VIRT_PAGE_PD_MASK
#define VIRT_PAGE_PT_MASK       0x000003FFUL
#define VIRT_PAGE_PT_INDEX_MASK (VIRT_PAGE_PD_INDEX_MASK | VIRT_PAGE_PT_MASK)

#define PAGE_TABLE_RCRS_SLOT    1023UL
#define PAGE_TABLE_RCRS_PT_BASE (PAGE_TABLE_RCRS_SLOT << 22)
#define PAGE_TABLE_RCRS_PD_BASE (PAGE_TABLE_RCRS_PT_BASE | (PAGE_TABLE_RCRS_SLOT << 12))

static union StA_PageDirectoryEntry *const _pd = (void *)PAGE_TABLE_RCRS_PD_BASE;
static union StA_PageTableEntry *const _pt = (void *)PAGE_TABLE_RCRS_PT_BASE;

StStatus StMmP_InitBaseAddressSpace(void)
{
    return STATUS_SUCCESS;
}

StStatus StMmP_VirtPageToPhysFrame(St_VirtPage vpn, St_PhysFrame *pfn)
{
    if (vpn > VIRT_PAGE_MAX) return STATUS_INVALID_VALUE;

    uint32_t pde_idx = (vpn & VIRT_PAGE_PD_INDEX_MASK) >> 10;
    if (!_pd[pde_idx].p) return STATUS_PAGE_NOT_PRESENT;
    if (_pd[pde_idx].ps) {
        return (_pd[pde_idx].huge.base_low << 10) + (vpn & 0x3FF);
    }

    uint32_t pte_idx = vpn & VIRT_PAGE_PT_INDEX_MASK;
    if (!_pt[pte_idx].p) return STATUS_PAGE_NOT_PRESENT;

    if (pfn) *pfn = _pt[pte_idx].base;

    return STATUS_SUCCESS;
}

StStatus StMmP_MapMemory(St_PhysFrame pfn, St_VirtPage vpn, uint32_t flags)
{
    StStatus status;
    union StA_PageTableEntry *pt = (void *)(0xFFC00000 + ((vpn & 0x000FFC00) << 2));
    St_PhysFrame new_pt_pfn;

    if (!_pc_page_dir[(vpn & 0x000FFC00) >> 10].p) {
        // create a new page table
        status = StPmm_AllocateFrame(&new_pt_pfn, 1, PMM_DEFAULT);
        if (!CHECK_SUCCESS(status)) return status;

        _pc_page_dir[(vpn & 0x000FFC00) >> 10].raw = 0x00000003 | (new_pt_pfn << 12);

        invalidate_page((uintptr_t)pt >> 12);

        for (int i = 0; i < 1024; i++) {
            pt[i].raw = 0x00000000;
        }
    }

    if (pt[vpn & 0x000003FF].p) {
        return STATUS_CONFLICTING_STATE;
    }

    pt[vpn & 0x000003FF].raw = pfn << 12;

    if (!(flags & MAP_READONLY)) {
        pt[vpn & 0x000003FF].r_w = 1;
    }

    if (flags & MAP_USER) {
        pt[vpn & 0x000003FF].u_s = 1;
    }

    if ((flags & MAP_NO_CACHE) || (flags & MAP_WRITE_THROUGH_CACHE)) {
        pt[vpn & 0x000003FF].pat = 1;

        if (flags & MAP_NO_CACHE) {
            pt[vpn & 0x000003FF].pcd = 1;
        }

        if (flags & MAP_WRITE_THROUGH_CACHE) {
            pt[vpn & 0x000003FF].pwt = 1;
        }
    }

    pt[vpn & 0x000003FF].p = 1;

    invalidate_page(vpn);

    return STATUS_SUCCESS;
}

void StMmP_UnmapMemory(St_PhysFrame pfn, St_VirtPage vpn, uint32_t flags)
{
    union StA_PageTableEntry *pt = (void *)(0xFFC00000 + ((vpn & 0x000FFC00) << 2));

    if (!_pc_page_dir[(vpn & 0x000FFC00) >> 10].p) return;
    if (!pt[vpn & 0x000003FF].p) return;

    pt[vpn & 0x000003FF].raw = 0;

    invalidate_page(vpn);
}
