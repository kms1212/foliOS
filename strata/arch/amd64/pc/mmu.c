#include <strata/plat/mmu.h>

#include <stdlib.h>
#include <string.h>

#include <strata/arch/cpufeatures.h>
#include <strata/arch/intrinsics/register.h>
#include <strata/arch/mmu.h>

#include <strata/plat/cpulocal.h>

#include <strata/macros.h>
#include <strata/types.h>

#define PML4_HOLE_START ((St_VirtPage)0x0000000800000000ULL)
#define PML4_HOLE_END   ((St_VirtPage)0x000FFFF7FFFFFFFFULL)

#define DIRECT_MAP_BASE  (0xFFFFC00000000000ULL)
#define PHYS_TO_VIRT(pa) ((void *)((uintptr_t)(pa) + DIRECT_MAP_BASE))

#define VIRT_PAGE_MAX       ((St_VirtPage)0x000FFFFFFFFFFFFFUL)
#define VIRT_PAGE_PML4_MASK ((St_VirtPage)0x0000000FF8000000UL)
#define VIRT_PAGE_PDPT_MASK ((St_VirtPage)0x0000000007FC0000UL)
#define VIRT_PAGE_PD_MASK   ((St_VirtPage)0x000000000003FE00UL)
#define VIRT_PAGE_PT_MASK   ((St_VirtPage)0x00000000000001FFUL)

struct StMmuP_AddressSpace base_asp;

static struct StMmuP_AddressSpace *first_asp = &base_asp;
static struct StMmuP_AddressSpace *last_asp = &base_asp;

StStatus StMmuP_Init(void)
{
    base_asp.next = NULL;
    base_asp.root_table_pfn = StA_ReadCr3() >> 12;

    StA_WriteCr3(StA_ReadCr3());

    return STATUS_SUCCESS;
}

StStatus StMmuP_LateInit(void)
{
    /* unmap lower direct mapping */
    union StA_PageMapLevel4Entry *base_pml4 = PHYS_TO_VIRT(FRAME_TO_VPTR(base_asp.root_table_pfn));

    union StA_PageDirPtrTableEntry *pdpt = PHYS_TO_VIRT(FRAME_TO_VPTR(base_pml4[0].base));

    base_pml4[0].raw = 0;
    for (int i = 0; i < 512; i++) {
        if (!pdpt[i].p) continue;

        union StA_PageDirPtrTableEntry *pd = PHYS_TO_VIRT(FRAME_TO_VPTR(pdpt[i].base));
        for (int j = 0; j < 512; j++) {
            if (!pd[j].p) continue;

            union StA_PaePageDirectoryEntry *pt = PHYS_TO_VIRT(FRAME_TO_VPTR(pd[j].base));
            for (int k = 0; k < 512; k++) {
                if (!pt[k].p) continue;

                StA_InvalidatePage((St_VirtPage)((i << 18) + (j << 9) + k));
            }
        }
    }

    return STATUS_SUCCESS;
}

StStatus StMmuP_CreateAddressSpace(struct StMmuP_AddressSpace **asp __out)
{
    StStatus status;
    struct StMmuP_AddressSpace *new_asp = NULL;
    St_PhysFrame root_table_pfn = (St_PhysFrame)-1;
    union StA_PageMapLevel4Entry *base_pml4 = PHYS_TO_VIRT(base_asp.root_table_pfn);

    new_asp = calloc(1, sizeof(*new_asp));
    if (!new_asp) {
        status = STATUS_UNKNOWN_ERROR;
        goto has_error;
    }

    status = StPmm_AllocateContiguousFrame(&root_table_pfn, (St_PageCount)1, PMM_DEFAULT);
    if (!CHECK_SUCCESS(status)) goto has_error;

    memcpy(PHYS_TO_VIRT(FRAME_TO_VPTR(root_table_pfn)), base_pml4, PAGE_SIZE);

    new_asp->root_table_pfn = root_table_pfn;

    last_asp->next = new_asp;
    last_asp = new_asp;

    *asp = new_asp;

    return STATUS_SUCCESS;

has_error:
    if (root_table_pfn != (St_PhysFrame)-1) {
        StPmm_FreeContiguousFrame(root_table_pfn);
    }

    if (new_asp) {
        free(new_asp);
    }

    return status;
}

StStatus StMmuP_RemoveAddressSpace(struct StMmuP_AddressSpace *asp __in)
{
    St_PhysFrame root_table_pfn = asp->root_table_pfn;

    StPmm_FreeContiguousFrame(root_table_pfn);

    free(asp);

    return STATUS_SUCCESS;
}

StStatus StMmuP_SwitchAddressSpace(struct StMmuP_AddressSpace *asp __in)
{
    StA_WriteCr3(asp->root_table_pfn << 12);

    return STATUS_SUCCESS;
}

StStatus StMmuP_VirtPageToPhysFrame(St_VirtPage vpn __in, St_PhysFrame *pfn __out_optional)
{
    struct StMmuP_AddressSpace *current_asp = StCpuLocalP_GetData()->current_asp;
    union StA_PageMapLevel4Entry *pml4;
    union StA_PageDirPtrTableEntry *pdpt;
    union StA_PaePageDirectoryEntry *pd;
    union StA_PaePageTableEntry *pt;
    uint64_t pml4e_idx;
    uint64_t pdpte_idx;
    uint64_t pde_idx;
    uint64_t pte_idx;

    if (vpn > VIRT_PAGE_MAX) return STATUS_INVALID_VALUE;
    if (PML4_HOLE_START <= vpn && vpn <= PML4_HOLE_END) return STATUS_PAGE_NOT_PRESENT;

    pml4 = PHYS_TO_VIRT(FRAME_TO_VPTR(current_asp->root_table_pfn));
    pml4e_idx = (vpn & VIRT_PAGE_PML4_MASK) >> 27;
    if (!pml4[pml4e_idx].p) return STATUS_PAGE_NOT_PRESENT;

    pdpt = PHYS_TO_VIRT(FRAME_TO_VPTR(pml4[pml4e_idx].base));
    pdpte_idx = (vpn & VIRT_PAGE_PDPT_MASK) >> 18;
    if (!pdpt[pdpte_idx].p) return STATUS_PAGE_NOT_PRESENT;
    if (pdpt[pdpte_idx].ps) {
        if (pfn) *pfn = (pdpt[pdpte_idx].huge.base << 18) + (vpn & 0x3FFFF);
        return STATUS_SUCCESS;
    }

    pd = PHYS_TO_VIRT(FRAME_TO_VPTR(pdpt[pdpte_idx].base));
    pde_idx = (vpn & VIRT_PAGE_PD_MASK) >> 9;
    if (!pd[pde_idx].p) return STATUS_PAGE_NOT_PRESENT;
    if (pd[pde_idx].ps) {
        if (pfn) *pfn = (pd[pde_idx].huge.base << 9) + (vpn & 0x1FF);
        return STATUS_SUCCESS;
    }

    pt = PHYS_TO_VIRT(FRAME_TO_VPTR(pd[pde_idx].base));
    pte_idx = vpn & VIRT_PAGE_PT_MASK;
    if (!pt[pte_idx].p) return STATUS_PAGE_NOT_PRESENT;

    if (pfn) *pfn = (St_PhysFrame)pt[pte_idx].base;

    return STATUS_SUCCESS;
}

StStatus StMmuP_MapMemory(St_PhysFrame pfn __in, St_VirtPage vpn __in, StMm_MapFlags mapflags __in)
{
    StStatus status;
    struct StMmuP_AddressSpace *current_asp = StCpuLocalP_GetData()->current_asp;
    union StA_PageMapLevel4Entry *pml4;
    union StA_PageDirPtrTableEntry *pdpt;
    union StA_PaePageDirectoryEntry *pd;
    union StA_PaePageTableEntry *pt;
    uint64_t pml4e_idx;
    uint64_t pdpte_idx;
    uint64_t pde_idx;
    uint64_t pte_idx;

    if (vpn > VIRT_PAGE_MAX) return STATUS_INVALID_VALUE;
    if (PML4_HOLE_START <= vpn && vpn <= PML4_HOLE_END) return STATUS_INVALID_VALUE;

    /* 1. PML4 */
    pml4 = PHYS_TO_VIRT(FRAME_TO_VPTR(current_asp->root_table_pfn));
    pml4e_idx = (vpn & VIRT_PAGE_PML4_MASK) >> 27;
    if (!pml4[pml4e_idx].p) {
        St_PhysFrame table_pfn;
        void *pdpt_page;

        status = StPmm_AllocateContiguousFrame(&table_pfn, (St_PageCount)1, PMM_DEFAULT);
        if (!CHECK_SUCCESS(status)) return status;

        pml4[pml4e_idx].raw = 0;
        pml4[pml4e_idx].base = (uint64_t)table_pfn;
        pml4[pml4e_idx].p = 1;
        pml4[pml4e_idx].r_w = 1;
        pml4[pml4e_idx].u_s = (mapflags & MAP_USER) ? 1 : 0;

        pdpt_page = PHYS_TO_VIRT(FRAME_TO_VPTR(pml4[pml4e_idx].base));
        StA_InvalidatePage(VPTR_TO_PAGE(pdpt_page));
        memset(pdpt_page, 0, PAGE_SIZE);
    } else {
        if (mapflags & MAP_USER) pml4[pml4e_idx].u_s = 1;
    }

    /* 2. PDPT */
    pdpt = PHYS_TO_VIRT(FRAME_TO_VPTR(pml4[pml4e_idx].base));
    pdpte_idx = (vpn & VIRT_PAGE_PDPT_MASK) >> 18;
    if (!pdpt[pdpte_idx].p) {
        St_PhysFrame table_pfn;
        void *pd_page;

        status = StPmm_AllocateContiguousFrame(&table_pfn, (St_PageCount)1, PMM_DEFAULT);
        if (!CHECK_SUCCESS(status)) return status;

        pdpt[pdpte_idx].raw = 0;
        pdpt[pdpte_idx].base = (uint64_t)table_pfn;
        pdpt[pdpte_idx].p = 1;
        pdpt[pdpte_idx].r_w = 1;
        pdpt[pdpte_idx].u_s = (mapflags & MAP_USER) ? 1 : 0;

        pd_page = PHYS_TO_VIRT(FRAME_TO_VPTR(pdpt[pdpte_idx].base));
        StA_InvalidatePage(VPTR_TO_PAGE(pd_page));
        memset(pd_page, 0, PAGE_SIZE);
    } else {
        if (pdpt[pdpte_idx].ps) return STATUS_CONFLICTING_STATE;
        if (mapflags & MAP_USER) pdpt[pdpte_idx].u_s = 1;
    }

    /* 3. PD */
    pd = PHYS_TO_VIRT(FRAME_TO_VPTR(pdpt[pdpte_idx].base));
    pde_idx = (vpn & VIRT_PAGE_PD_MASK) >> 9;
    if (!pd[pde_idx].p) {
        St_PhysFrame table_pfn;
        void *pt_page;

        status = StPmm_AllocateContiguousFrame(&table_pfn, (St_PageCount)1, PMM_DEFAULT);
        if (!CHECK_SUCCESS(status)) return status;

        pd[pde_idx].raw = 0;
        pd[pde_idx].base = (uint64_t)table_pfn;
        pd[pde_idx].p = 1;
        pd[pde_idx].r_w = 1;
        pd[pde_idx].u_s = (mapflags & MAP_USER) ? 1 : 0;

        pt_page = PHYS_TO_VIRT(FRAME_TO_VPTR(pd[pde_idx].base));
        StA_InvalidatePage(VPTR_TO_PAGE(pt_page));
        memset(pt_page, 0, PAGE_SIZE);
    } else {
        if (pd[pde_idx].ps) return STATUS_CONFLICTING_STATE;
        if (mapflags & MAP_USER) pd[pde_idx].u_s = 1;
    }

    /* 4. PT */
    pt = PHYS_TO_VIRT(FRAME_TO_VPTR(pd[pde_idx].base));
    pte_idx = vpn & VIRT_PAGE_PT_MASK;
    if (pt[pte_idx].p) return STATUS_DUPLICATE_ENTRY;

    pt[pte_idx].raw = 0;
    pt[pte_idx].base = (uint64_t)pfn;
    pt[pte_idx].p = 1;

    pt[pte_idx].r_w = (mapflags & MAP_READONLY) ? 0 : 1;
    pt[pte_idx].u_s = (mapflags & MAP_USER) ? 1 : 0;
    pt[pte_idx].pcd = (mapflags & MAP_NO_CACHE) ? 1 : 0;
    pt[pte_idx].pwt = (mapflags & MAP_WRITETHRU_CACHE) ? 1 : 0;

    if (g_p_cpu_features->has_nx) {
        pt[pte_idx].xd = (mapflags & MAP_NO_EXECUTE) ? 1 : 0;
    }

    StA_InvalidatePage(vpn);

    return STATUS_SUCCESS;
}

StStatus StMmuP_RemapMemory(St_VirtPage vpn __in, StMm_MapFlags mapflags __in)
{
    struct StMmuP_AddressSpace *current_asp = StCpuLocalP_GetData()->current_asp;
    union StA_PageMapLevel4Entry *pml4;
    union StA_PageDirPtrTableEntry *pdpt;
    union StA_PaePageDirectoryEntry *pd;
    union StA_PaePageTableEntry *pt;
    uint64_t pml4e_idx;
    uint64_t pdpte_idx;
    uint64_t pde_idx;
    uint64_t pte_idx;

    if (vpn > VIRT_PAGE_MAX) return STATUS_INVALID_VALUE;
    if (PML4_HOLE_START <= vpn && vpn <= PML4_HOLE_END) return STATUS_INVALID_VALUE;

    /* 1. PML4 */
    pml4 = PHYS_TO_VIRT(FRAME_TO_VPTR(current_asp->root_table_pfn));
    pml4e_idx = (vpn & VIRT_PAGE_PML4_MASK) >> 27;
    if (!pml4[pml4e_idx].p) {
        return STATUS_PAGE_NOT_PRESENT;
    }

    if (mapflags & MAP_USER) pml4[pml4e_idx].u_s = 1;

    /* 2. PDPT */
    pdpt = PHYS_TO_VIRT(FRAME_TO_VPTR(pml4[pml4e_idx].base));
    pdpte_idx = (vpn & VIRT_PAGE_PDPT_MASK) >> 18;
    if (!pdpt[pdpte_idx].p) {
        return STATUS_PAGE_NOT_PRESENT;
    }

    if (mapflags & MAP_USER) pdpt[pdpte_idx].u_s = 1;

    /* 3. PD */
    pd = PHYS_TO_VIRT(FRAME_TO_VPTR(pdpt[pdpte_idx].base));
    pde_idx = (vpn & VIRT_PAGE_PD_MASK) >> 9;
    if (!pd[pde_idx].p) {
        return STATUS_PAGE_NOT_PRESENT;
    }

    if (pd[pde_idx].ps) return STATUS_CONFLICTING_STATE;
    if (mapflags & MAP_USER) pd[pde_idx].u_s = 1;

    /* 4. PT */
    pt = PHYS_TO_VIRT(FRAME_TO_VPTR(pd[pde_idx].base));
    pte_idx = vpn & VIRT_PAGE_PT_MASK;
    if (!pt[pte_idx].p) return STATUS_PAGE_NOT_PRESENT;

    pt[pte_idx].r_w = (mapflags & MAP_READONLY) ? 0 : 1;
    pt[pte_idx].u_s = (mapflags & MAP_USER) ? 1 : 0;
    pt[pte_idx].pcd = (mapflags & MAP_NO_CACHE) ? 1 : 0;
    pt[pte_idx].pwt = (mapflags & MAP_WRITETHRU_CACHE) ? 1 : 0;

    if (g_p_cpu_features->has_nx) {
        pt[pte_idx].xd = (mapflags & MAP_NO_EXECUTE) ? 1 : 0;
    }

    StA_InvalidatePage(vpn);

    return STATUS_SUCCESS;
}

void StMmuP_UnmapMemory(St_VirtPage vpn) {}
