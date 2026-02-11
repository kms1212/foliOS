#include <strata/plat/mm.h>

#include <inttypes.h>
#include <stdlib.h>
#include <strata/status.h>
#include <strata/thread.h>
#include <string.h>

#include <strata/arch/cpufeatures.h>
#include <strata/arch/intrinsics/register.h>
#include <strata/arch/mmu.h>

#include <strata/plat/cpulocal.h>
#include <strata/plat/memmap.h>

#include <strata/log.h>
#include <strata/macros.h>
#include <strata/panic.h>
#include <strata/types.h>

#include <strata/mm.h>

#define MODULE_NAME "mm"

#define PML4_HOLE_START ((St_VirtPage)0x0000000800000000ULL)
#define PML4_HOLE_END   ((St_VirtPage)0x000FFFF7FFFFFFFFULL)

#define PHYS_TO_VIRT(pa) ((void *)((uintptr_t)(pa) + MEMMAP_DIRECTMAP_VPN_BASE * PAGE_SIZE))

#define VIRT_PAGE_MAX       ((St_VirtPage)0x000FFFFFFFFFFFFFUL)
#define VIRT_PAGE_PML4_MASK ((St_VirtPage)0x0000000FF8000000UL)
#define VIRT_PAGE_PDPT_MASK ((St_VirtPage)0x0000000007FC0000UL)
#define VIRT_PAGE_PD_MASK   ((St_VirtPage)0x000000000003FE00UL)
#define VIRT_PAGE_PT_MASK   ((St_VirtPage)0x00000000000001FFUL)

#define PML4_INDEX(vpn) (((vpn) & VIRT_PAGE_PML4_MASK) >> 27)
#define PDPT_INDEX(vpn) (((vpn) & VIRT_PAGE_PDPT_MASK) >> 18)
#define PD_INDEX(vpn)   (((vpn) & VIRT_PAGE_PD_MASK) >> 9)
#define PT_INDEX(vpn)   ((vpn) & VIRT_PAGE_PT_MASK)

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

    status = StPmm_AllocateContiguousFrame(&root_table_pfn, (St_PageCount)1, NULL, AF_DEFAULT);
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
    union StA_PageMapLevel4Entry *pml4 = PHYS_TO_VIRT(FRAME_TO_VPTR(root_table_pfn));

    /*
     * Iterate over the user half of the address space (entries 0-255).
     * The kernel half (256-511) is shared and should not be freed here.
     */
    for (int i = 0; i < 256; i++) {
        if (!pml4[i].p) continue;

        St_PhysFrame pdpt_pfn = (St_PhysFrame)pml4[i].base;
        union StA_PageDirPtrTableEntry *pdpt = PHYS_TO_VIRT(FRAME_TO_VPTR(pdpt_pfn));

        for (int j = 0; j < 512; j++) {
            if (!pdpt[j].p) continue;

            /* We don't support 1GB pages in user space yet, but check anyway */
            if (pdpt[j].ps) continue;

            St_PhysFrame pd_pfn = (St_PhysFrame)pdpt[j].base;
            union StA_PaePageDirectoryEntry *pd = PHYS_TO_VIRT(FRAME_TO_VPTR(pd_pfn));

            for (int k = 0; k < 512; k++) {
                if (!pd[k].p) continue;

                /* We don't support 2MB pages in user space yet, but check anyway */
                if (pd[k].ps) continue;

                St_PhysFrame pt_pfn = (St_PhysFrame)pd[k].base;
                StPmm_FreeContiguousFrame(pt_pfn);
            }
            StPmm_FreeContiguousFrame(pd_pfn);
        }
        StPmm_FreeContiguousFrame(pdpt_pfn);
    }

    StPmm_FreeContiguousFrame(root_table_pfn);
}

StStatus StMmP_SwitchAddressSpace(struct StMm_AddressSpace *asp __in)
{
    StA_WriteCr3(asp->platform_data.root_table_pfn << 12);

    StCpuLocalP_GetData()->current_asp = asp;

    return STATUS_SUCCESS;
}

static StStatus vpn_to_pfn(
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
    pml4e_idx = PML4_INDEX(vpn);
    if (!pml4[pml4e_idx].p) return STATUS_PAGE_NOT_PRESENT;

    pdpt = PHYS_TO_VIRT(FRAME_TO_VPTR(pml4[pml4e_idx].base));
    pdpte_idx = PDPT_INDEX(vpn);
    if (!pdpt[pdpte_idx].p) return STATUS_PAGE_NOT_PRESENT;
    if (pdpt[pdpte_idx].ps) {
        if (pfn) *pfn = (pdpt[pdpte_idx].huge.base << 18) + (vpn & 0x3FFFF);
        return STATUS_SUCCESS;
    }

    pd = PHYS_TO_VIRT(FRAME_TO_VPTR(pdpt[pdpte_idx].base));
    pde_idx = PD_INDEX(vpn);
    if (!pd[pde_idx].p) return STATUS_PAGE_NOT_PRESENT;
    if (pd[pde_idx].ps) {
        if (pfn) *pfn = (pd[pde_idx].huge.base << 9) + (vpn & 0x1FF);
        return STATUS_SUCCESS;
    }

    pt = PHYS_TO_VIRT(FRAME_TO_VPTR(pd[pde_idx].base));
    pte_idx = PT_INDEX(vpn);
    if (!pt[pte_idx].p) return STATUS_PAGE_NOT_PRESENT;

    if (pfn) *pfn = (St_PhysFrame)pt[pte_idx].base;

    return STATUS_SUCCESS;
}

StStatus StMmP_GlobalVirtPageToPhysFrame(St_VirtPage vpn __in, St_PhysFrame *pfn __out_optional)
{
    if (!IS_GLOBAL_VPN(vpn)) return STATUS_INVALID_VALUE;

    return vpn_to_pfn(&base_asp, vpn, pfn);
}

StStatus StMmP_LocalVirtPageToPhysFrame(
    struct StMm_AddressSpace *asp __in, St_VirtPage vpn __in, St_PhysFrame *pfn __out_optional
)
{
    if (!IS_LOCAL_VPN(vpn)) return STATUS_INVALID_VALUE;

    return vpn_to_pfn(asp, vpn, pfn);
}

static StStatus map_memory(
    struct StMm_AddressSpace *asp __in,
    St_PhysFrame pfn __in,
    St_VirtPage vpn __in,
    St_PageCount count __in,
    StMm_MapFlags mapflags __in
)
{
    StStatus status;
    union StA_PageMapLevel4Entry *base_pml4 =
        PHYS_TO_VIRT(FRAME_TO_VPTR(base_asp.platform_data.root_table_pfn));
    union StA_PageMapLevel4Entry *pml4;
    union StA_PageDirPtrTableEntry *pdpt;
    union StA_PaePageDirectoryEntry *pd;
    union StA_PaePageTableEntry *pt;
    uint64_t pml4e_idx;
    uint64_t pdpte_idx;
    uint64_t pde_idx;
    uint64_t pte_idx;
    size_t chunk_size;
    St_PhysFrame alloc_pfn;
    union StA_PaePageTableEntry pte_template;

    if (vpn + count > VIRT_PAGE_MAX) return STATUS_INVALID_VALUE;
    if (PML4_HOLE_START <= vpn && vpn <= PML4_HOLE_END) return STATUS_INVALID_VALUE;
    if (PML4_HOLE_START <= vpn + count && vpn + count - 1 <= PML4_HOLE_END) {
        return STATUS_INVALID_VALUE;
    }

    // LOG_DEBUG(
    //     LM_CAT_UNCLASSIFIED,
    //     "mapping %" PRIX64 "(%" PRIu64 " page(s)) to %" PRIX64 ", flags: %" PRIX32 "\n",
    //     vpn,
    //     count,
    //     pfn,
    //     mapflags
    // );

    pte_template.raw = 0;
    pte_template.p = 1;
    pte_template.r_w = (mapflags & MF_WRITABLE) ? 1 : 0;
    pte_template.u_s = (mapflags & MF_USER) ? 1 : 0;
    pte_template.pcd = (mapflags & MF_NO_CACHE) ? 1 : 0;
    pte_template.pwt = (mapflags & MF_WRITETHRU_CACHE) ? 1 : 0;
    if (g_p_cpu_features->has_nx) {
        pte_template.xd = (mapflags & MF_NO_EXECUTE) ? 1 : 0;
    }

    if (vpn >= MEMMAP_GLOBAL_VPN_BASE) {
        StThread_LockPreemption();
    }

    pml4 = PHYS_TO_VIRT(FRAME_TO_VPTR(asp->platform_data.root_table_pfn));

    while (count > 0) {
        /* 1. PML4 */
        pml4e_idx = PML4_INDEX(vpn);
        if (!pml4[pml4e_idx].p) {

            if (vpn >= MEMMAP_GLOBAL_VPN_BASE && !pml4[pml4e_idx].p && base_pml4[pml4e_idx].p) {
                pml4[pml4e_idx].raw = base_pml4[pml4e_idx].raw;

                pdpt = PHYS_TO_VIRT(FRAME_TO_VPTR(pml4[pml4e_idx].base));
            } else {
                status =
                    StPmm_AllocateContiguousFrame(&alloc_pfn, (St_PageCount)1, NULL, AF_DEFAULT);
                if (!CHECK_SUCCESS(status)) goto has_error;

                pdpt = PHYS_TO_VIRT(FRAME_TO_VPTR(alloc_pfn));
                memset(pdpt, 0, PAGE_SIZE);

                union StA_PageMapLevel4Entry temp;

                temp.raw = 0;
                temp.base = (uint64_t)alloc_pfn;
                temp.p = 1;
                temp.r_w = 1;
                temp.u_s = (mapflags & MF_USER) ? 1 : 0;

                if (vpn >= MEMMAP_GLOBAL_VPN_BASE) {
                    if (base_pml4[pml4e_idx].p) {
                        StPmm_FreeContiguousFrame(alloc_pfn);

                        pml4[pml4e_idx].raw = base_pml4[pml4e_idx].raw;

                        pdpt = PHYS_TO_VIRT(FRAME_TO_VPTR(base_pml4[pml4e_idx].base));
                    } else {
                        base_pml4[pml4e_idx].raw = temp.raw;
                        pml4[pml4e_idx].raw = temp.raw;
                    }
                } else {
                    pml4[pml4e_idx].raw = temp.raw;
                }
            }
        } else {
            if (mapflags & MF_USER) pml4[pml4e_idx].u_s = 1;
            pdpt = PHYS_TO_VIRT(FRAME_TO_VPTR(pml4[pml4e_idx].base));
        }

        /* 2. PDPT */
        pdpte_idx = PDPT_INDEX(vpn);
        if (!pdpt[pdpte_idx].p) {
            status = StPmm_AllocateContiguousFrame(&alloc_pfn, (St_PageCount)1, NULL, AF_DEFAULT);
            if (!CHECK_SUCCESS(status)) goto has_error;

            pd = PHYS_TO_VIRT(FRAME_TO_VPTR(alloc_pfn));
            memset(pd, 0, PAGE_SIZE);

            pdpt[pdpte_idx].raw = 0;
            pdpt[pdpte_idx].base = (uint64_t)alloc_pfn;
            pdpt[pdpte_idx].p = 1;
            pdpt[pdpte_idx].r_w = 1;
            pdpt[pdpte_idx].u_s = (mapflags & MF_USER) ? 1 : 0;
        } else {
            if (pdpt[pdpte_idx].ps) {
                status = STATUS_CONFLICTING_STATE;
                goto has_error;
            }
            if (mapflags & MF_USER) pdpt[pdpte_idx].u_s = 1;
            pd = PHYS_TO_VIRT(FRAME_TO_VPTR(pdpt[pdpte_idx].base));
        }

        /* 3. PD */
        pde_idx = PD_INDEX(vpn);
        if (!pd[pde_idx].p) {
            status = StPmm_AllocateContiguousFrame(&alloc_pfn, (St_PageCount)1, NULL, AF_DEFAULT);
            if (!CHECK_SUCCESS(status)) goto has_error;

            pt = PHYS_TO_VIRT(FRAME_TO_VPTR(alloc_pfn));
            memset(pt, 0, PAGE_SIZE);

            pd[pde_idx].raw = 0;
            pd[pde_idx].base = (uint64_t)alloc_pfn;
            pd[pde_idx].p = 1;
            pd[pde_idx].r_w = 1;
            pd[pde_idx].u_s = (mapflags & MF_USER) ? 1 : 0;
        } else {
            if (pd[pde_idx].ps) {
                status = STATUS_CONFLICTING_STATE;
                goto has_error;
            }
            if (mapflags & MF_USER) pd[pde_idx].u_s = 1;
            pt = PHYS_TO_VIRT(FRAME_TO_VPTR(pd[pde_idx].base));
        }

        /* 4. PT */
        pte_idx = PT_INDEX(vpn);

        chunk_size = 512 - pte_idx;
        if (count < chunk_size) {
            chunk_size = count;
        }

        for (size_t i = 0; i < chunk_size; i++) {
            if (pt[pte_idx + i].p) {
                status = STATUS_DUPLICATE_ENTRY;
                goto has_error;
            }
            pt[pte_idx + i] = pte_template;
            pt[pte_idx + i].base = (uint64_t)pfn + i;
        }

        count -= chunk_size;
        vpn += chunk_size;
        pfn += chunk_size;
    }

    if (vpn >= MEMMAP_GLOBAL_VPN_BASE) {
        StThread_UnlockPreemption();
    }

    return STATUS_SUCCESS;

has_error:
    if (vpn >= MEMMAP_GLOBAL_VPN_BASE) {
        StThread_UnlockPreemption();
    }

    return status;
}

StStatus StMmP_MapGlobalContiguousMemory(
    St_PhysFrame pfn __in,
    St_VirtPage vpn __in,
    St_PageCount count __in,
    StMm_MapFlags mapflags __in
)
{
    if (!IS_GLOBAL_VPN(vpn)) return STATUS_INVALID_VALUE;

    return map_memory(&base_asp, pfn, vpn, count, mapflags);
}

StStatus StMmP_MapLocalContiguousMemory(
    struct StMm_AddressSpace *asp __in,
    St_PhysFrame pfn __in,
    St_VirtPage vpn __in,
    St_PageCount count __in,
    StMm_MapFlags mapflags __in
)
{
    if (!IS_LOCAL_VPN(vpn)) return STATUS_INVALID_VALUE;

    return map_memory(asp, pfn, vpn, count, mapflags);
}

static StStatus remap_memory(
    struct StMm_AddressSpace *asp __in,
    St_VirtPage vpn __in,
    St_PageCount count __in,
    StMm_MapFlags mapflags __in
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
    size_t chunk_size;
    union StA_PaePageTableEntry pte_template;
    St_PhysFrame pfn;
    int do_invlpg;

    if (vpn > VIRT_PAGE_MAX) return STATUS_INVALID_VALUE;
    if (PML4_HOLE_START <= vpn && vpn <= PML4_HOLE_END) return STATUS_INVALID_VALUE;
    if (PML4_HOLE_START <= vpn + count && vpn + count - 1 <= PML4_HOLE_END) {
        return STATUS_INVALID_VALUE;
    }

    if (g_p_cpu_features->has_invlpg && count < 16) {
        do_invlpg = 1;
    } else {
        do_invlpg = 0;
    }

    pte_template.raw = 0;
    pte_template.p = 1;
    pte_template.r_w = (mapflags & MF_WRITABLE) ? 1 : 0;
    pte_template.u_s = (mapflags & MF_USER) ? 1 : 0;
    pte_template.pcd = (mapflags & MF_NO_CACHE) ? 1 : 0;
    pte_template.pwt = (mapflags & MF_WRITETHRU_CACHE) ? 1 : 0;
    if (g_p_cpu_features->has_nx) {
        pte_template.xd = (mapflags & MF_NO_EXECUTE) ? 1 : 0;
    }

    if (vpn >= MEMMAP_GLOBAL_VPN_BASE) {
        StThread_LockPreemption();
    }

    pml4 = PHYS_TO_VIRT(FRAME_TO_VPTR(asp->platform_data.root_table_pfn));

    while (count > 0) {
        /* 1. PML4 */
        pml4e_idx = PML4_INDEX(vpn);
        if (!pml4[pml4e_idx].p) {
            return STATUS_PAGE_NOT_PRESENT;
        }

        if (mapflags & MF_USER) pml4[pml4e_idx].u_s = 1;
        pdpt = PHYS_TO_VIRT(FRAME_TO_VPTR(pml4[pml4e_idx].base));

        /* 2. PDPT */
        pdpte_idx = PDPT_INDEX(vpn);
        if (!pdpt[pdpte_idx].p) {
            return STATUS_PAGE_NOT_PRESENT;
        }

        if (mapflags & MF_USER) pdpt[pdpte_idx].u_s = 1;
        pd = PHYS_TO_VIRT(FRAME_TO_VPTR(pdpt[pdpte_idx].base));

        /* 3. PD */
        pde_idx = PD_INDEX(vpn);
        if (!pd[pde_idx].p) {
            return STATUS_PAGE_NOT_PRESENT;
        }

        if (pd[pde_idx].ps) return STATUS_CONFLICTING_STATE;
        if (mapflags & MF_USER) pd[pde_idx].u_s = 1;
        pt = PHYS_TO_VIRT(FRAME_TO_VPTR(pd[pde_idx].base));

        /* 4. PT */
        pte_idx = vpn & VIRT_PAGE_PT_MASK;

        chunk_size = 512 - pte_idx;
        if (count < chunk_size) {
            chunk_size = count;
        }

        for (size_t i = 0; i < chunk_size; i++) {
            if (!pt[pte_idx + i].p) return STATUS_PAGE_NOT_PRESENT;
            pfn = pt[pte_idx + i].base;
            pt[pte_idx + i] = pte_template;
            pt[pte_idx + i].base = pfn;

            if (do_invlpg) {
                StA_InvalidatePage(vpn + i);
            }
        }

        count -= chunk_size;
        vpn += chunk_size;
    }

    if (!do_invlpg) {
        StA_WriteCr3(StA_ReadCr3());
    }

    if (vpn >= MEMMAP_GLOBAL_VPN_BASE) {
        StThread_UnlockPreemption();
    }

    return STATUS_SUCCESS;
}

StStatus StMmP_RemapGlobalContiguousMemory(
    St_VirtPage vpn __in, St_PageCount count __in, StMm_MapFlags mapflags __in
)
{
    if (!IS_GLOBAL_VPN(vpn)) return STATUS_INVALID_VALUE;

    return remap_memory(&base_asp, vpn, count, mapflags);
}

StStatus StMmP_RemapLocalContiguousMemory(
    struct StMm_AddressSpace *asp __in,
    St_VirtPage vpn __in,
    St_PageCount count __in,
    StMm_MapFlags mapflags __in
)
{
    if (!IS_LOCAL_VPN(vpn)) return STATUS_INVALID_VALUE;

    return remap_memory(asp, vpn, count, mapflags);
}

static void unmap_memory(
    struct StMm_AddressSpace *asp __in, St_VirtPage vpn __in, St_PageCount count __in
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
    size_t chunk_size;
    int do_invlpg;

    if (vpn > VIRT_PAGE_MAX) return;
    if (PML4_HOLE_START <= vpn && vpn <= PML4_HOLE_END) return;
    if (PML4_HOLE_START <= vpn + count && vpn + count - 1 <= PML4_HOLE_END) {
        return;
    }

    if (g_p_cpu_features->has_invlpg && count < 16) {
        do_invlpg = 1;
    } else {
        do_invlpg = 0;
    }

    if (vpn >= MEMMAP_GLOBAL_VPN_BASE) {
        StThread_LockPreemption();
    }

    pml4 = PHYS_TO_VIRT(FRAME_TO_VPTR(asp->platform_data.root_table_pfn));

    while (count > 0) {
        /* 1. PML4 */
        pml4e_idx = PML4_INDEX(vpn);
        if (!pml4[pml4e_idx].p) {
            St_Panic(STATUS_PAGE_NOT_PRESENT, "PML4 entry not present");
        }
        pdpt = PHYS_TO_VIRT(FRAME_TO_VPTR(pml4[pml4e_idx].base));

        /* 2. PDPT */
        pdpte_idx = PDPT_INDEX(vpn);
        if (!pdpt[pdpte_idx].p) {
            St_Panic(STATUS_PAGE_NOT_PRESENT, "PDPT entry not present");
        }
        pd = PHYS_TO_VIRT(FRAME_TO_VPTR(pdpt[pdpte_idx].base));

        /* 3. PD */
        pde_idx = PD_INDEX(vpn);
        if (!pd[pde_idx].p) {
            St_Panic(STATUS_PAGE_NOT_PRESENT, "PD entry not present");
        }
        pt = PHYS_TO_VIRT(FRAME_TO_VPTR(pd[pde_idx].base));

        /* 4. PT */
        pte_idx = vpn & VIRT_PAGE_PT_MASK;

        chunk_size = 512 - pte_idx;
        if (count < chunk_size) {
            chunk_size = count;
        }

        for (size_t i = 0; i < chunk_size; i++) {
            if (!pt[pte_idx + i].p) {
                St_Panic(STATUS_PAGE_NOT_PRESENT, "UnmapMemory: PT entry not present");
            }
            pt[pte_idx + i].raw = 0;

            if (do_invlpg) {
                StA_InvalidatePage(vpn + i);
            }
        }

        count -= chunk_size;
        vpn += chunk_size;
    }

    if (!do_invlpg) {
        StA_WriteCr3(StA_ReadCr3());
    }

    if (vpn >= MEMMAP_GLOBAL_VPN_BASE) {
        StThread_UnlockPreemption();
    }
}

void StMmP_UnmapGlobalContiguousMemory(St_VirtPage vpn __in, St_PageCount count __in)
{
    if (!IS_GLOBAL_VPN(vpn)) return;

    unmap_memory(&base_asp, vpn, count);
}

void StMmP_UnmapLocalContiguousMemory(
    struct StMm_AddressSpace *asp __in, St_VirtPage vpn __in, St_PageCount count __in
)
{
    if (!IS_LOCAL_VPN(vpn)) return;

    unmap_memory(asp, vpn, count);
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
