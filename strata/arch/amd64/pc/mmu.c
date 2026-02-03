#include "strata/types.h"
#include <strata/plat/mmu.h>

#include <stdlib.h>
#include <string.h>

#include <strata/arch/cpufeatures.h>
#include <strata/arch/intrinsics/register.h>
#include <strata/arch/mmu.h>

#include <strata/macros.h>

#define PML4_HOLE_START ((St_VirtPage)0x0000000800000000ULL)
#define PML4_HOLE_END   ((St_VirtPage)0x000FFFF7FFFFFFFFULL)

#define VIRT_PAGE_MAX             ((St_VirtPage)0x000FFFFFFFFFFFFFUL)
#define VIRT_PAGE_PML4_MASK       ((St_VirtPage)0x0000000FF8000000UL)
#define VIRT_PAGE_PML4_INDEX_MASK VIRT_PAGE_PML4_MASK
#define VIRT_PAGE_PDPT_MASK       ((St_VirtPage)0x0000000007FC0000UL)
#define VIRT_PAGE_PDPT_INDEX_MASK (VIRT_PAGE_PML4_INDEX_MASK | VIRT_PAGE_PDPT_MASK)
#define VIRT_PAGE_PD_MASK         ((St_VirtPage)0x000000000003FE00UL)
#define VIRT_PAGE_PD_INDEX_MASK   (VIRT_PAGE_PDPT_INDEX_MASK | VIRT_PAGE_PD_MASK)
#define VIRT_PAGE_PT_MASK         ((St_VirtPage)0x00000000000001FFUL)
#define VIRT_PAGE_PT_INDEX_MASK   (VIRT_PAGE_PD_INDEX_MASK | VIRT_PAGE_PT_MASK)

#define PAGE_TABLE_RCRS_SLOT      510UL
#define PAGE_TABLE_RCRS_PT_BASE   (0xFFFF000000000000ULL | (PAGE_TABLE_RCRS_SLOT << 39))
#define PAGE_TABLE_RCRS_PD_BASE   (PAGE_TABLE_RCRS_PT_BASE | (PAGE_TABLE_RCRS_SLOT << 30))
#define PAGE_TABLE_RCRS_PDPT_BASE (PAGE_TABLE_RCRS_PD_BASE | (PAGE_TABLE_RCRS_SLOT << 21))
#define PAGE_TABLE_RCRS_PML4_BASE (PAGE_TABLE_RCRS_PDPT_BASE | (PAGE_TABLE_RCRS_SLOT << 12))

static union StA_PageMapLevel4Entry *const _pml4 = (void *)PAGE_TABLE_RCRS_PML4_BASE;
static union StA_PageDirPtrTableEntry *const _pdpt = (void *)PAGE_TABLE_RCRS_PDPT_BASE;
static union StA_PaePageDirectoryEntry *const _pd = (void *)PAGE_TABLE_RCRS_PD_BASE;
static union StA_PaePageTableEntry *const _pt = (void *)PAGE_TABLE_RCRS_PT_BASE;

static struct StMmuP_AddressSpace base_asp;

static struct StMmuP_AddressSpace *current_asp = &base_asp;
static struct StMmuP_AddressSpace *first_asp = &base_asp;
static struct StMmuP_AddressSpace *last_asp = &base_asp;

/* 8 TiB */
static union StA_PageDirPtrTableEntry direct_mapping_pdpt[16][512] __aligned(4096);

StStatus StMmuP_Init(void)
{
    StStatus status;
    St_PhysFrame pfn;

    base_asp.next = NULL;
    base_asp.root_table_pfn = StA_ReadCr3() >> 12;

    /* setup direct mapping */
    for (int i = 0; i < ARRAY_SIZE(direct_mapping_pdpt); i++) {
        status = StMmuP_VirtPageToPhysFrame(VPTR_TO_PAGE(&direct_mapping_pdpt[i]), &pfn);
        if (status != STATUS_SUCCESS) return status;

        _pml4[i + 384].p = 1;
        _pml4[i + 384].r_w = 1;
        _pml4[i + 384].base = (uint64_t)pfn;

        for (int j = 0; j < ARRAY_SIZE(direct_mapping_pdpt[i]); j++) {
            direct_mapping_pdpt[i][j].p = 1;
            direct_mapping_pdpt[i][j].r_w = 1;
            direct_mapping_pdpt[i][j].ps = 1;
            direct_mapping_pdpt[i][j].base = i * 512 + j;
        }
    }

    StA_WriteCr3(StA_ReadCr3());

    return STATUS_SUCCESS;
}

StStatus StMmuP_LateInit(void)
{
    /* unmap lower direct mapping */
    _pml4[0].raw = 0;

    StA_InvalidatePage((St_VirtPage)0);

    return STATUS_SUCCESS;
}

StStatus StMmuP_CreateAddressSpace(struct StMmuP_AddressSpace **asp __out)
{
    StStatus status;
    struct StMmuP_AddressSpace *new_asp = NULL;
    St_PhysFrame root_table_pfn = (St_PhysFrame)-1;

    new_asp = calloc(1, sizeof(*new_asp));
    if (!new_asp) {
        status = STATUS_UNKNOWN_ERROR;
        goto has_error;
    }

    status = StPmm_AllocateContiguousFrame(&root_table_pfn, (St_PageCount)1, PMM_DEFAULT);
    if (!CHECK_SUCCESS(status)) goto has_error;

    memcpy(
        PAGE_TO_VPTR((St_VirtPage)0xFFFFC00000000ULL + (St_VirtPage)root_table_pfn),
        _pml4,
        PAGE_SIZE
    );

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
    uint64_t pml4e_idx;
    uint64_t pdpte_idx;
    uint64_t pde_idx;
    uint64_t pte_idx;

    if (vpn > VIRT_PAGE_MAX) return STATUS_INVALID_VALUE;
    if (PML4_HOLE_START <= vpn && vpn <= PML4_HOLE_END) return STATUS_PAGE_NOT_PRESENT;

    pml4e_idx = (vpn & VIRT_PAGE_PML4_INDEX_MASK) >> 27;
    if (!_pml4[pml4e_idx].p) return STATUS_PAGE_NOT_PRESENT;

    pdpte_idx = (vpn & VIRT_PAGE_PDPT_INDEX_MASK) >> 18;
    if (!_pdpt[pdpte_idx].p) return STATUS_PAGE_NOT_PRESENT;
    if (_pdpt[pdpte_idx].ps) {
        if (pfn) *pfn = (_pdpt[pdpte_idx].huge.base << 18) + (vpn & 0x3FFFF);
        return STATUS_SUCCESS;
    }

    pde_idx = (vpn & VIRT_PAGE_PD_INDEX_MASK) >> 9;
    if (!_pd[pde_idx].p) return STATUS_PAGE_NOT_PRESENT;
    if (_pd[pde_idx].ps) {
        if (pfn) *pfn = (_pd[pde_idx].huge.base << 9) + (vpn & 0x1FF);
        return STATUS_SUCCESS;
    }

    pte_idx = vpn & VIRT_PAGE_PT_INDEX_MASK;
    if (!_pt[pte_idx].p) return STATUS_PAGE_NOT_PRESENT;

    if (pfn) *pfn = (St_PhysFrame)_pt[pte_idx].base;

    return STATUS_SUCCESS;
}

StStatus StMmuP_MapMemory(St_PhysFrame pfn __in, St_VirtPage vpn __in, StMm_MapFlags mapflags __in)
{
    StStatus status;
    uint64_t pml4e_idx;
    uint64_t pdpte_idx;
    uint64_t pde_idx;
    uint64_t pte_idx;

    if (vpn > VIRT_PAGE_MAX) return STATUS_INVALID_VALUE;
    if (PML4_HOLE_START <= vpn && vpn <= PML4_HOLE_END) return STATUS_INVALID_VALUE;

    /* 1. PML4 */
    pml4e_idx = (vpn & VIRT_PAGE_PML4_INDEX_MASK) >> 27;
    if (!_pml4[pml4e_idx].p) {
        St_PhysFrame table_pfn;
        void *pdpt_page;

        status = StPmm_AllocateContiguousFrame(&table_pfn, (St_PageCount)1, PMM_DEFAULT);
        if (!CHECK_SUCCESS(status)) return status;

        _pml4[pml4e_idx].raw = 0;
        _pml4[pml4e_idx].base = (uint64_t)table_pfn;
        _pml4[pml4e_idx].p = 1;
        _pml4[pml4e_idx].r_w = 1;
        _pml4[pml4e_idx].u_s = (mapflags & MAP_USER) ? 1 : 0;

        pdpt_page = (void *)&_pdpt[pml4e_idx << 9];
        StA_InvalidatePage(VPTR_TO_PAGE(pdpt_page));
        memset(pdpt_page, 0, PAGE_SIZE);
    } else {
        if (mapflags & MAP_USER) _pml4[pml4e_idx].u_s = 1;
    }

    /* 2. PDPT */
    pdpte_idx = (vpn & VIRT_PAGE_PDPT_INDEX_MASK) >> 18;
    if (!_pdpt[pdpte_idx].p) {
        St_PhysFrame table_pfn;
        void *pd_page;

        status = StPmm_AllocateContiguousFrame(&table_pfn, (St_PageCount)1, PMM_DEFAULT);
        if (!CHECK_SUCCESS(status)) return status;

        _pdpt[pdpte_idx].raw = 0;
        _pdpt[pdpte_idx].base = (uint64_t)table_pfn;
        _pdpt[pdpte_idx].p = 1;
        _pdpt[pdpte_idx].r_w = 1;
        _pdpt[pdpte_idx].u_s = (mapflags & MAP_USER) ? 1 : 0;

        pd_page = (void *)&_pd[pdpte_idx << 9];
        StA_InvalidatePage(VPTR_TO_PAGE(pd_page));
        memset(pd_page, 0, PAGE_SIZE);
    } else {
        if (_pdpt[pdpte_idx].ps) return STATUS_CONFLICTING_STATE;
        if (mapflags & MAP_USER) _pdpt[pdpte_idx].u_s = 1;
    }

    /* 3. PD */
    pde_idx = (vpn & VIRT_PAGE_PD_INDEX_MASK) >> 9;
    if (!_pd[pde_idx].p) {
        St_PhysFrame table_pfn;
        void *pt_page;

        status = StPmm_AllocateContiguousFrame(&table_pfn, (St_PageCount)1, PMM_DEFAULT);
        if (!CHECK_SUCCESS(status)) return status;

        _pd[pde_idx].raw = 0;
        _pd[pde_idx].base = (uint64_t)table_pfn;
        _pd[pde_idx].p = 1;
        _pd[pde_idx].r_w = 1;
        _pd[pde_idx].u_s = (mapflags & MAP_USER) ? 1 : 0;

        pt_page = (void *)&_pt[pde_idx << 9];
        StA_InvalidatePage(VPTR_TO_PAGE(pt_page));
        memset(pt_page, 0, PAGE_SIZE);
    } else {
        if (_pd[pde_idx].ps) return STATUS_CONFLICTING_STATE;
        if (mapflags & MAP_USER) _pd[pde_idx].u_s = 1;
    }

    /* 4. PT */
    pte_idx = vpn & VIRT_PAGE_PT_INDEX_MASK;
    if (_pt[pte_idx].p) return STATUS_DUPLICATE_ENTRY;

    _pt[pte_idx].raw = 0;
    _pt[pte_idx].base = (uint64_t)pfn;
    _pt[pte_idx].p = 1;

    _pt[pte_idx].r_w = (mapflags & MAP_READONLY) ? 0 : 1;
    _pt[pte_idx].u_s = (mapflags & MAP_USER) ? 1 : 0;
    _pt[pte_idx].pcd = (mapflags & MAP_NO_CACHE) ? 1 : 0;
    _pt[pte_idx].pwt = (mapflags & MAP_WRITETHRU_CACHE) ? 1 : 0;

    if (g_p_cpu_features->has_nx && 0) {
        _pt[pte_idx].xd = (mapflags & MAP_NO_EXECUTE) ? 1 : 0;
    }

    StA_InvalidatePage(vpn);

    return STATUS_SUCCESS;
}

StStatus StMmuP_RemapMemory(St_VirtPage vpn __in, StMm_MapFlags mapflags __in)
{
    uint64_t pml4e_idx;
    uint64_t pdpte_idx;
    uint64_t pde_idx;
    uint64_t pte_idx;

    if (vpn > VIRT_PAGE_MAX) return STATUS_INVALID_VALUE;
    if (PML4_HOLE_START <= vpn && vpn <= PML4_HOLE_END) return STATUS_INVALID_VALUE;

    /* 1. PML4 */
    pml4e_idx = (vpn & VIRT_PAGE_PML4_INDEX_MASK) >> 27;
    if (!_pml4[pml4e_idx].p) {
        return STATUS_PAGE_NOT_PRESENT;
    }

    if (mapflags & MAP_USER) _pml4[pml4e_idx].u_s = 1;

    /* 2. PDPT */
    pdpte_idx = (vpn & VIRT_PAGE_PDPT_INDEX_MASK) >> 18;
    if (!_pdpt[pdpte_idx].p) {
        return STATUS_PAGE_NOT_PRESENT;
    }

    if (mapflags & MAP_USER) _pdpt[pdpte_idx].u_s = 1;

    /* 3. PD */
    pde_idx = (vpn & VIRT_PAGE_PD_INDEX_MASK) >> 9;
    if (!_pd[pde_idx].p) {
        return STATUS_PAGE_NOT_PRESENT;
    }

    if (_pd[pde_idx].ps) return STATUS_CONFLICTING_STATE;
    if (mapflags & MAP_USER) _pd[pde_idx].u_s = 1;

    /* 4. PT */
    pte_idx = vpn & VIRT_PAGE_PT_INDEX_MASK;
    if (!_pt[pte_idx].p) return STATUS_PAGE_NOT_PRESENT;

    _pt[pte_idx].r_w = (mapflags & MAP_READONLY) ? 0 : 1;
    _pt[pte_idx].u_s = (mapflags & MAP_USER) ? 1 : 0;
    _pt[pte_idx].pcd = (mapflags & MAP_NO_CACHE) ? 1 : 0;
    _pt[pte_idx].pwt = (mapflags & MAP_WRITETHRU_CACHE) ? 1 : 0;

    if (g_p_cpu_features->has_nx && 0) {
        _pt[pte_idx].xd = (mapflags & MAP_NO_EXECUTE) ? 1 : 0;
    }

    StA_InvalidatePage(vpn);

    return STATUS_SUCCESS;
}

void StMmuP_UnmapMemory(St_VirtPage vpn) {}
