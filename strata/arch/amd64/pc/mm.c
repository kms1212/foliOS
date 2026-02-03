#include <strata/plat/mm.h>

#include <stdlib.h>
#include <string.h>

#include <strata/arch/cpufeatures.h>
#include <strata/arch/intrinsics/register.h>
#include <strata/arch/mmu.h>

#include <strata/plat/cpulocal.h>
#include <strata/plat/memmap.h>

#include <strata/macros.h>
#include <strata/types.h>

#include <strata/mm.h>

#define PML4_HOLE_START ((St_VirtPage)0x0000000800000000ULL)
#define PML4_HOLE_END   ((St_VirtPage)0x000FFFF7FFFFFFFFULL)

#define DIRECT_MAP_BASE  (0xFFFFC00000000000ULL)
#define PHYS_TO_VIRT(pa) ((void *)((uintptr_t)(pa) + DIRECT_MAP_BASE))

#define VIRT_PAGE_MAX       ((St_VirtPage)0x000FFFFFFFFFFFFFUL)
#define VIRT_PAGE_PML4_MASK ((St_VirtPage)0x0000000FF8000000UL)
#define VIRT_PAGE_PDPT_MASK ((St_VirtPage)0x0000000007FC0000UL)
#define VIRT_PAGE_PD_MASK   ((St_VirtPage)0x000000000003FE00UL)
#define VIRT_PAGE_PT_MASK   ((St_VirtPage)0x00000000000001FFUL)

extern struct StMm_AddressSpace base_asp;

StStatus StMmP_InitBaseAddressSpace(void)
{
    base_asp.platform_data.root_table_pfn = StA_ReadCr3() >> 12;

    return STATUS_SUCCESS;
}

StStatus StMmP_CleanupTempMapping(void)
{
    /* unmap lower direct mapping */
    union StA_PageMapLevel4Entry *base_pml4 =
        PHYS_TO_VIRT(FRAME_TO_VPTR(base_asp.platform_data.root_table_pfn));

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

StStatus StMmP_CreateAddressSpace(struct StMm_AddressSpace *asp __in)
{
    StStatus status;
    St_PhysFrame root_table_pfn = (St_PhysFrame)-1;
    union StA_PageMapLevel4Entry *base_pml4 =
        PHYS_TO_VIRT(FRAME_TO_VPTR(base_asp.platform_data.root_table_pfn));

    status = StPmm_AllocateContiguousFrame(&root_table_pfn, (St_PageCount)1, PMM_DEFAULT);
    if (!CHECK_SUCCESS(status)) goto has_error;

    memcpy(PHYS_TO_VIRT(FRAME_TO_VPTR(root_table_pfn)), base_pml4, PAGE_SIZE);

    asp->platform_data.root_table_pfn = root_table_pfn;

    return STATUS_SUCCESS;

has_error:
    if (root_table_pfn != (St_PhysFrame)-1) {
        StPmm_FreeContiguousFrame(root_table_pfn);
    }

    return status;
}

void StMmP_RemoveAddressSpace(struct StMm_AddressSpace *asp __in)
{
    St_PhysFrame root_table_pfn = asp->platform_data.root_table_pfn;

    StPmm_FreeContiguousFrame(root_table_pfn);
}

StStatus StMmP_SwitchAddressSpace(struct StMm_AddressSpace *asp __in)
{
    StA_WriteCr3(asp->platform_data.root_table_pfn << 12);

    StCpuLocalP_GetData()->current_asp = asp;

    return STATUS_SUCCESS;
}

static StStatus StMmP_VirtPageToPhysFrame(
    struct StMm_AddressSpace *asp __in, St_VirtPage vpn __in, St_PhysFrame *pfn __out_optional
)
{
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

    pml4 = PHYS_TO_VIRT(FRAME_TO_VPTR(asp->platform_data.root_table_pfn));
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

StStatus StMmP_GlobalVirtPageToPhysFrame(St_VirtPage vpn __in, St_PhysFrame *pfn __out_optional)
{
    if (!IS_GLOBAL_VPN(vpn)) return STATUS_INVALID_VALUE;

    return StMmP_VirtPageToPhysFrame(&base_asp, vpn, pfn);
}

StStatus StMmP_LocalVirtPageToPhysFrame(
    struct StMm_AddressSpace *asp __in, St_VirtPage vpn __in, St_PhysFrame *pfn __out_optional
)
{
    if (!IS_LOCAL_VPN(vpn)) return STATUS_INVALID_VALUE;

    return StMmP_VirtPageToPhysFrame(asp, vpn, pfn);
}

static StStatus StMmP_MapMemory(
    struct StMm_AddressSpace *asp __in,
    St_PhysFrame pfn __in,
    St_VirtPage vpn __in,
    StMm_MapFlags mapflags __in
)
{
    StStatus status;
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
    pml4 = PHYS_TO_VIRT(FRAME_TO_VPTR(asp->platform_data.root_table_pfn));
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

StStatus StMmP_MapGlobalMemory(
    St_PhysFrame pfn __in, St_VirtPage vpn __in, StMm_MapFlags mapflags __in
)
{
    if (!IS_GLOBAL_VPN(vpn)) return STATUS_INVALID_VALUE;

    return StMmP_MapMemory(&base_asp, pfn, vpn, mapflags);
}

StStatus StMmP_MapLocalMemory(
    struct StMm_AddressSpace *asp __in,
    St_PhysFrame pfn __in,
    St_VirtPage vpn __in,
    StMm_MapFlags mapflags __in
)
{
    if (!IS_LOCAL_VPN(vpn)) return STATUS_INVALID_VALUE;

    return StMmP_MapMemory(asp, pfn, vpn, mapflags);
}

static StStatus StMmP_RemapMemory(
    struct StMm_AddressSpace *asp __in, St_VirtPage vpn __in, StMm_MapFlags mapflags __in
)
{
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
    pml4 = PHYS_TO_VIRT(FRAME_TO_VPTR(asp->platform_data.root_table_pfn));
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

StStatus StMmP_RemapGlobalMemory(St_VirtPage vpn __in, StMm_MapFlags mapflags __in)
{
    if (!IS_GLOBAL_VPN(vpn)) return STATUS_INVALID_VALUE;

    return StMmP_RemapMemory(&base_asp, vpn, mapflags);
}

StStatus StMmP_RemapLocalMemory(
    struct StMm_AddressSpace *asp __in, St_VirtPage vpn __in, StMm_MapFlags mapflags __in
)
{
    if (!IS_LOCAL_VPN(vpn)) return STATUS_INVALID_VALUE;

    return StMmP_RemapMemory(asp, vpn, mapflags);
}

static void StMmP_UnmapMemory(struct StMm_AddressSpace *asp __in, St_VirtPage vpn __in) {}

void StMmP_UnmapGlobalMemory(St_VirtPage vpn __in)
{
    if (!IS_GLOBAL_VPN(vpn)) return;

    StMmP_UnmapMemory(&base_asp, vpn);
}

void StMmP_UnmapLocalMemory(struct StMm_AddressSpace *asp __in, St_VirtPage vpn __in)
{
    if (!IS_LOCAL_VPN(vpn)) return;

    StMmP_UnmapMemory(asp, vpn);
}

StStatus StMmP_ReadLocal(
    struct StMm_AddressSpace *asp __in, uintptr_t addr __in, void *buf __buf, size_t len __in
)
{
    StStatus status;
    St_PhysFrame pfn;
    St_VirtPage vpn;
    uintptr_t copy_offset;
    size_t copy_size;
    uint8_t *bbuf = buf;

    while (len > 0) {
        copy_offset = addr & (PAGE_SIZE - 1);
        copy_size = PAGE_SIZE - copy_offset;
        if (copy_size > len) copy_size = len;

        vpn = ADDR_TO_PAGE(addr);

        status = StMmP_LocalVirtPageToPhysFrame(asp, vpn, &pfn);
        if (!CHECK_SUCCESS(status)) return status;

        memcpy(bbuf, (uint8_t *)PHYS_TO_VIRT(FRAME_TO_VPTR(pfn)) + copy_offset, copy_size);

        addr += copy_size;
        bbuf += copy_size;
        len -= copy_size;
    }

    return STATUS_SUCCESS;
}

StStatus StMmP_WriteLocal(
    struct StMm_AddressSpace *asp __in, uintptr_t addr __in, const void *buf __in, size_t len __in
)
{
    StStatus status;
    St_PhysFrame pfn;
    St_VirtPage vpn;
    uintptr_t copy_offset;
    size_t copy_size;
    const uint8_t *bbuf = buf;

    while (len > 0) {
        copy_offset = addr & (PAGE_SIZE - 1);
        copy_size = PAGE_SIZE - copy_offset;
        if (copy_size > len) copy_size = len;

        vpn = ADDR_TO_PAGE(addr);

        status = StMmP_LocalVirtPageToPhysFrame(asp, vpn, &pfn);
        if (!CHECK_SUCCESS(status)) return status;

        memcpy((uint8_t *)PHYS_TO_VIRT(FRAME_TO_VPTR(pfn)) + copy_offset, bbuf, copy_size);

        addr += copy_size;
        bbuf += copy_size;
        len -= copy_size;
    }

    return STATUS_SUCCESS;
}

StStatus StMmP_SetLocal(
    struct StMm_AddressSpace *asp __in, uintptr_t addr __in, int value, size_t len __in
)
{
    StStatus status;
    St_PhysFrame pfn;
    St_VirtPage vpn;
    uintptr_t copy_offset;
    size_t copy_size;

    while (len > 0) {
        copy_offset = addr & (PAGE_SIZE - 1);
        copy_size = PAGE_SIZE - copy_offset;
        if (copy_size > len) copy_size = len;

        vpn = ADDR_TO_PAGE(addr);

        status = StMmP_LocalVirtPageToPhysFrame(asp, vpn, &pfn);
        if (!CHECK_SUCCESS(status)) return status;

        memset((uint8_t *)PHYS_TO_VIRT(FRAME_TO_VPTR(pfn)) + copy_offset, value, copy_size);

        addr += copy_size;
        len -= copy_size;
    }

    return STATUS_SUCCESS;
}

StStatus StMmP_CopyLocal(
    struct StMm_AddressSpace *dest_asp __in,
    uintptr_t dest __in,
    struct StMm_AddressSpace *src_asp __in,
    uintptr_t src __in,
    size_t len __in
)
{
    // TODO: implement
    return STATUS_UNIMPLEMENTED;
}
