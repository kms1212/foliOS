#include <strata/plat/mm.h>

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <strata/arch/cpufeatures.h>
#include <strata/arch/intrinsics/register.h>
#include <strata/arch/mmu.h>
#include <strata/arch/mmu_constants.h>

#include <strata/plat/cpulocal.h>
#include <strata/plat/memmap.h>

#include <strata/compiler.h>
#include <strata/mm.h>
#include <strata/mm/address_space.h>
#include <strata/mm/address_space_refs.h>
#include <strata/mm/pmm.h>
#include <strata/mm/types.h>
#include <strata/mm/vmm.h>
#include <strata/panic.h>
#include <strata/status.h>
#include <strata/thread.h>

#define MODULE_NAME                               "mm"
#define PAGE_TABLE_FRAME_CACHE_MAX_PAGES          ((St_PageCount)128)
#define PAGE_TABLE_FRAME_CACHE_LOW_FREE_WATERMARK ((St_PageCount)4096)

#define PML4_HOLE_START ((St_VirtPage)0x0000000800000000ULL)
#define PML4_HOLE_END   ((St_VirtPage)0x000FFFF7FFFFFFFFULL)

#define PHYS_TO_VIRT(pa) ((void *)((uintptr_t)(pa) + (MEMMAP_DIRECTMAP_VPN_BASE * PAGE_SIZE)))

#define VIRT_PAGE_MAX       ((St_VirtPage)0x000FFFFFFFFFFFFFUL)
#define VIRT_PAGE_PML4_MASK ((St_VirtPage)0x0000000FF8000000UL)
#define VIRT_PAGE_PDPT_MASK ((St_VirtPage)0x0000000007FC0000UL)
#define VIRT_PAGE_PD_MASK   ((St_VirtPage)0x000000000003FE00UL)
#define VIRT_PAGE_PT_MASK   ((St_VirtPage)0x00000000000001FFUL)

#define PML4_INDEX(vpn) (((vpn) & VIRT_PAGE_PML4_MASK) >> 27)
#define PDPT_INDEX(vpn) (((vpn) & VIRT_PAGE_PDPT_MASK) >> 18)
#define PD_INDEX(vpn)   (((vpn) & VIRT_PAGE_PD_MASK) >> 9)
#define PT_INDEX(vpn)   ((vpn) & VIRT_PAGE_PT_MASK)

extern struct StAddressSpace base_asp;

struct cached_page_table_frame {
    struct cached_page_table_frame *next;
};

static struct cached_page_table_frame *page_table_frame_cache_head = NULL;
static St_PageCount page_table_frame_cache_pages = 0;
static struct cached_page_table_frame *page_table_frame_quarantine_head = NULL;
static St_PageCount page_table_frame_quarantine_pages = 0;

static struct cached_page_table_frame *pop_cached_page_table_frame(
    struct cached_page_table_frame **head, St_PageCount *page_count
)
{
    struct cached_page_table_frame *cached_frame = *head;

    if (!cached_frame) return NULL;

    *head = cached_frame->next;
    if (*page_count) {
        (*page_count)--;
    }

    return cached_frame;
}

static void release_quarantined_page_table_frames(void)
{
    struct cached_page_table_frame *quarantine_head;
    struct cached_page_table_frame *quarantine_tail;
    St_PageCount quarantine_pages;

    StThread_LockPreemption();

    quarantine_head = page_table_frame_quarantine_head;
    quarantine_pages = page_table_frame_quarantine_pages;
    page_table_frame_quarantine_head = NULL;
    page_table_frame_quarantine_pages = 0;

    if (!quarantine_head) {
        StThread_UnlockPreemption();
        return;
    }

    quarantine_tail = quarantine_head;
    while (quarantine_tail->next) {
        quarantine_tail = quarantine_tail->next;
    }

    quarantine_tail->next = page_table_frame_cache_head;
    page_table_frame_cache_head = quarantine_head;
    page_table_frame_cache_pages += quarantine_pages;

    StThread_UnlockPreemption();
}

static StA_PaePageTableEntry mapflags_to_pte_private_bits(StMm_MapFlags mapflags)
{
    StA_PaePageTableEntry bits = 0;

    if (mapflags & MF_POOL_LARGE_ALLOC) bits |= PTE_SW0;
    if (mapflags & MF_POOL_SUBPOOL) bits |= PTE_SW1;

    return bits;
}

static StMm_MapFlags pte_to_mapflags(StA_PaePageTableEntry pte)
{
    StMm_MapFlags mapflags = 0;

    if (pte & PTE_RW) mapflags |= MF_WRITABLE;
    if (pte & PTE_US) mapflags |= MF_USER;
    if (pte & PTE_PCD) mapflags |= MF_NO_CACHE;
    if (pte & PTE_PWT) mapflags |= MF_WRITETHRU_CACHE;
    if (pte & PTE_G) mapflags |= MF_GLOBAL;
    if (g_p_cpu_features->has_nx && (pte & PTE_XD)) mapflags |= MF_NO_EXECUTE;
    if (pte & PTE_SW0) mapflags |= MF_POOL_LARGE_ALLOC;
    if (pte & PTE_SW1) mapflags |= MF_POOL_SUBPOOL;

    return mapflags;
}

static __always_inline St_PhysFrame directmap_ptr_to_pfn(const void *ptr)
{
    return ADDR_TO_FRAME((uintptr_t)ptr - PAGE_TO_ADDR(MEMMAP_DIRECTMAP_VPN_BASE));
}

static StStatus allocate_fresh_page_table_frame(St_PhysFrame *pfn)
{
    StStatus status;

    status = StPmm_AllocateContiguousFrame(pfn, (St_PageCount)1, NULL, AF_DEFAULT);
    if (!CHECK_SUCCESS(status)) return status;

    memset(PHYS_TO_VIRT(FRAME_TO_VPTR(*pfn)), 0, PAGE_SIZE);

    return STATUS_SUCCESS;
}

static StStatus allocate_page_table_frame(St_PhysFrame *pfn, int allow_cache)
{
    StStatus status;
    struct cached_page_table_frame *cached_frame;

    if (!allow_cache) {
        return allocate_fresh_page_table_frame(pfn);
    }

    StThread_LockPreemption();

    cached_frame =
        pop_cached_page_table_frame(&page_table_frame_cache_head, &page_table_frame_cache_pages);

    StThread_UnlockPreemption();

    if (cached_frame) {
        *pfn = directmap_ptr_to_pfn(cached_frame);
        memset(cached_frame, 0, PAGE_SIZE);
        return STATUS_SUCCESS;
    }

    return allocate_fresh_page_table_frame(pfn);
}

static void free_page_table_frame(St_PhysFrame pfn)
{
    St_PageCount free_frames = 0;

    StPmm_GetFreeFrameCount(&free_frames);
    if (free_frames > PAGE_TABLE_FRAME_CACHE_LOW_FREE_WATERMARK) {
        StThread_LockPreemption();

        if (page_table_frame_cache_pages + page_table_frame_quarantine_pages <
            PAGE_TABLE_FRAME_CACHE_MAX_PAGES) {
            struct cached_page_table_frame *cached_frame =
                (struct cached_page_table_frame *)PHYS_TO_VIRT(FRAME_TO_VPTR(pfn));

            cached_frame->next = page_table_frame_quarantine_head;
            page_table_frame_quarantine_head = cached_frame;
            page_table_frame_quarantine_pages++;

            StThread_UnlockPreemption();
            return;
        }

        StThread_UnlockPreemption();
    }

    StPmm_FreeContiguousFrame(pfn);
}

St_PageCount StMmP_ReclaimCachedPageTableFrames(St_PageCount page_budget)
{
    St_PageCount reclaimed_pages = 0;

    while (page_budget == 0 || reclaimed_pages < page_budget) {
        struct cached_page_table_frame *cached_frame;

        StThread_LockPreemption();

        cached_frame = pop_cached_page_table_frame(
            &page_table_frame_cache_head,
            &page_table_frame_cache_pages
        );
        if (!cached_frame) {
            cached_frame = pop_cached_page_table_frame(
                &page_table_frame_quarantine_head,
                &page_table_frame_quarantine_pages
            );
        }

        StThread_UnlockPreemption();

        if (!cached_frame) break;

        StPmm_FreeContiguousFrame(directmap_ptr_to_pfn(cached_frame));
        reclaimed_pages++;
    }

    return reclaimed_pages;
}

StStatus StAddressSpaceP_InitBase(void)
{
    base_asp.platform_data.root_table_pfn = StA_ReadCr3() >> 12;

    return STATUS_SUCCESS;
}

StStatus StMmP_CleanupTempMapping(void)
{
    /* unmap lower direct mapping */
    StA_PageMapLevel4Entry *base_pml4 =
        PHYS_TO_VIRT(FRAME_TO_VPTR(base_asp.platform_data.root_table_pfn));

    StA_PageDirPtrTableEntry *pdpt =
        PHYS_TO_VIRT(FRAME_TO_VPTR(((base_pml4[0]) & PTE_BASE_MASK) >> PTE_BASE_SHIFT));

    base_pml4[0] = 0;
    for (int i = 0; i < 512; i++) {
        if (!(pdpt[i] & PTE_P)) continue;

        StA_PageDirPtrTableEntry *pd =
            PHYS_TO_VIRT(FRAME_TO_VPTR(((pdpt[i]) & PTE_BASE_MASK) >> PTE_BASE_SHIFT));
        for (int j = 0; j < 512; j++) {
            if (!(pd[j] & PTE_P)) continue;

            StA_PaePageDirectoryEntry *pt =
                PHYS_TO_VIRT(FRAME_TO_VPTR(((pd[j]) & PTE_BASE_MASK) >> PTE_BASE_SHIFT));
            for (int k = 0; k < 512; k++) {
                if (!(pt[k] & PTE_P)) continue;

                StA_InvalidatePage((i << 18) + (j << 9) + k);
            }
        }
    }

    return STATUS_SUCCESS;
}

StStatus StAddressSpaceP_Create(StAddressSpace_StrongRef asp __in)
{
    StStatus status;
    St_PhysFrame root_table_pfn = (St_PhysFrame)-1;
    StA_PageMapLevel4Entry *base_pml4 =
        PHYS_TO_VIRT(FRAME_TO_VPTR(base_asp.platform_data.root_table_pfn));

    status = allocate_fresh_page_table_frame(&root_table_pfn);
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

void StAddressSpaceP_Remove(StAddressSpace_StrongRef asp __in)
{
    St_PhysFrame root_table_pfn = asp->platform_data.root_table_pfn;
    StA_PageMapLevel4Entry *pml4 = PHYS_TO_VIRT(FRAME_TO_VPTR(root_table_pfn));

    /*
     * Iterate over the user half of the address space (entries 0-255).
     * The kernel half (256-511) is shared and should not be freed here.
     */
    for (int i = 0; i < 256; i++) {
        if (!(pml4[i] & PTE_P)) continue;

        St_PhysFrame pdpt_pfn = (St_PhysFrame)((pml4[i]) & PTE_BASE_MASK) >> PTE_BASE_SHIFT;
        StA_PageDirPtrTableEntry *pdpt = PHYS_TO_VIRT(FRAME_TO_VPTR(pdpt_pfn));

        for (int j = 0; j < 512; j++) {
            if (!(pdpt[j] & PTE_P)) continue;

            /* We don't support 1GB pages in user space yet, but check anyway */
            if ((pdpt[j] & PTE_PS)) continue;

            St_PhysFrame pd_pfn = (St_PhysFrame)((pdpt[j]) & PTE_BASE_MASK) >> PTE_BASE_SHIFT;
            StA_PaePageDirectoryEntry *pd = PHYS_TO_VIRT(FRAME_TO_VPTR(pd_pfn));

            for (int k = 0; k < 512; k++) {
                if (!(pd[k] & PTE_P)) continue;

                /* We don't support 2MB pages in user space yet, but check anyway */
                if ((pd[k] & PTE_PS)) continue;

                St_PhysFrame pt_pfn = (St_PhysFrame)((pd[k]) & PTE_BASE_MASK) >> PTE_BASE_SHIFT;
                StPmm_FreeContiguousFrame(pt_pfn);
            }
            StPmm_FreeContiguousFrame(pd_pfn);
        }
        StPmm_FreeContiguousFrame(pdpt_pfn);
    }

    StPmm_FreeContiguousFrame(root_table_pfn);
}

StStatus StAddressSpaceP_Switch(StAddressSpace_StrongRef asp __in)
{
    StA_WriteCr3(asp->platform_data.root_table_pfn << 12);
    release_quarantined_page_table_frames();

    StCpuLocalP_GetData()->current_asp = (StAddressSpace_InternalRef)asp;

    return STATUS_SUCCESS;
}

static StStatus vpn_to_pfn(
    StAddressSpace_StrongRef asp __in, St_VirtPage vpn __in, St_PhysFrame *pfn __out_optional
)
{
    StA_PageMapLevel4Entry *pml4;
    StA_PageDirPtrTableEntry *pdpt;
    StA_PaePageDirectoryEntry *pd;
    StA_PaePageTableEntry *pt;
    uint64_t pml4e_idx;
    uint64_t pdpte_idx;
    uint64_t pde_idx;
    uint64_t pte_idx;

    if (vpn > VIRT_PAGE_MAX) return STATUS_INVALID_VALUE;
    if (PML4_HOLE_START <= vpn && vpn <= PML4_HOLE_END) return STATUS_PAGE_NOT_PRESENT;

    pml4 = PHYS_TO_VIRT(FRAME_TO_VPTR(asp->platform_data.root_table_pfn));
    pml4e_idx = PML4_INDEX(vpn);
    if (!(pml4[pml4e_idx] & PTE_P)) return STATUS_PAGE_NOT_PRESENT;

    pdpt = PHYS_TO_VIRT(FRAME_TO_VPTR(((pml4[pml4e_idx]) & PTE_BASE_MASK) >> PTE_BASE_SHIFT));
    pdpte_idx = PDPT_INDEX(vpn);
    if (!(pdpt[pdpte_idx] & PTE_P)) return STATUS_PAGE_NOT_PRESENT;
    if ((pdpt[pdpte_idx] & PTE_PS)) {
        if (pfn) *pfn = ((pdpt[pdpte_idx] & PTE_BASE_MASK) >> PTE_BASE_SHIFT) + (vpn & 0x3FFFF);
        return STATUS_SUCCESS;
    }

    pd = PHYS_TO_VIRT(FRAME_TO_VPTR(((pdpt[pdpte_idx]) & PTE_BASE_MASK) >> PTE_BASE_SHIFT));
    pde_idx = PD_INDEX(vpn);
    if (!(pd[pde_idx] & PTE_P)) return STATUS_PAGE_NOT_PRESENT;
    if ((pd[pde_idx] & PTE_PS)) {
        if (pfn) *pfn = ((pd[pde_idx] & PTE_BASE_MASK) >> PTE_BASE_SHIFT) + (vpn & 0x1FF);
        return STATUS_SUCCESS;
    }

    pt = PHYS_TO_VIRT(FRAME_TO_VPTR(((pd[pde_idx]) & PTE_BASE_MASK) >> PTE_BASE_SHIFT));
    pte_idx = PT_INDEX(vpn);
    if (!(pt[pte_idx] & PTE_P)) return STATUS_PAGE_NOT_PRESENT;

    if (pfn) *pfn = (St_PhysFrame)((pt[pte_idx]) & PTE_BASE_MASK) >> PTE_BASE_SHIFT;

    return STATUS_SUCCESS;
}

static StStatus vpn_to_page_map_flags(
    StAddressSpace_StrongRef asp __in, St_VirtPage vpn __in, StMm_MapFlags *mapflags_out __out
)
{
    assert(mapflags_out);

    StA_PageMapLevel4Entry *pml4;
    StA_PageDirPtrTableEntry *pdpt;
    StA_PaePageDirectoryEntry *pd;
    StA_PaePageTableEntry *pt;
    StA_PaePageTableEntry entry;
    uint64_t pml4e_idx;
    uint64_t pdpte_idx;
    uint64_t pde_idx;
    uint64_t pte_idx;

    if (vpn > VIRT_PAGE_MAX) return STATUS_INVALID_VALUE;
    if (PML4_HOLE_START <= vpn && vpn <= PML4_HOLE_END) return STATUS_PAGE_NOT_PRESENT;

    pml4 = PHYS_TO_VIRT(FRAME_TO_VPTR(asp->platform_data.root_table_pfn));
    pml4e_idx = PML4_INDEX(vpn);
    if (!(pml4[pml4e_idx] & PTE_P)) return STATUS_PAGE_NOT_PRESENT;

    pdpt = PHYS_TO_VIRT(FRAME_TO_VPTR(((pml4[pml4e_idx]) & PTE_BASE_MASK) >> PTE_BASE_SHIFT));
    pdpte_idx = PDPT_INDEX(vpn);
    if (!(pdpt[pdpte_idx] & PTE_P)) return STATUS_PAGE_NOT_PRESENT;
    if ((pdpt[pdpte_idx] & PTE_PS)) {
        *mapflags_out = pte_to_mapflags(pdpt[pdpte_idx]);
        return STATUS_SUCCESS;
    }

    pd = PHYS_TO_VIRT(FRAME_TO_VPTR(((pdpt[pdpte_idx]) & PTE_BASE_MASK) >> PTE_BASE_SHIFT));
    pde_idx = PD_INDEX(vpn);
    if (!(pd[pde_idx] & PTE_P)) return STATUS_PAGE_NOT_PRESENT;
    if ((pd[pde_idx] & PTE_PS)) {
        *mapflags_out = pte_to_mapflags(pd[pde_idx]);
        return STATUS_SUCCESS;
    }

    pt = PHYS_TO_VIRT(FRAME_TO_VPTR(((pd[pde_idx]) & PTE_BASE_MASK) >> PTE_BASE_SHIFT));
    pte_idx = PT_INDEX(vpn);
    if (!(pt[pte_idx] & PTE_P)) return STATUS_PAGE_NOT_PRESENT;

    entry = pt[pte_idx];
    *mapflags_out = pte_to_mapflags(entry);

    return STATUS_SUCCESS;
}

static StStatus local_addr_to_directmap_span(
    StAddressSpace_StrongRef asp __in,
    uintptr_t addr __in,
    size_t max_len __in,
    uint8_t **direct_ptr __out,
    size_t *span_len __out
)
{
    assert(direct_ptr);
    assert(span_len);

    StA_PageMapLevel4Entry *pml4;
    StA_PageDirPtrTableEntry *pdpt;
    StA_PaePageDirectoryEntry *pd;
    StA_PaePageTableEntry *pt;
    St_VirtPage vpn = ADDR_TO_PAGE(addr);
    uintptr_t page_offset = addr & (PAGE_SIZE - 1);
    uint64_t pml4e_idx;
    uint64_t pdpte_idx;
    uint64_t pde_idx;
    uint64_t pte_idx;
    St_PhysFrame pfn;
    St_PageCount contiguous_pages;
    size_t available_len;

    if (!max_len) {
        *direct_ptr = NULL;
        *span_len = 0;
        return STATUS_SUCCESS;
    }

    if (vpn > VIRT_PAGE_MAX) return STATUS_INVALID_VALUE;
    if (PML4_HOLE_START <= vpn && vpn <= PML4_HOLE_END) return STATUS_PAGE_NOT_PRESENT;

    pml4 = PHYS_TO_VIRT(FRAME_TO_VPTR(asp->platform_data.root_table_pfn));
    pml4e_idx = PML4_INDEX(vpn);
    if (!(pml4[pml4e_idx] & PTE_P)) return STATUS_PAGE_NOT_PRESENT;

    pdpt = PHYS_TO_VIRT(FRAME_TO_VPTR(((pml4[pml4e_idx]) & PTE_BASE_MASK) >> PTE_BASE_SHIFT));
    pdpte_idx = PDPT_INDEX(vpn);
    if (!(pdpt[pdpte_idx] & PTE_P)) return STATUS_PAGE_NOT_PRESENT;
    if ((pdpt[pdpte_idx] & PTE_PS)) {
        pfn = ((pdpt[pdpte_idx] & PTE_BASE_MASK) >> PTE_BASE_SHIFT) + (vpn & 0x3FFFF);
        contiguous_pages = ((St_PageCount)1 << 18) - (vpn & 0x3FFFF);
        goto has_span;
    }

    pd = PHYS_TO_VIRT(FRAME_TO_VPTR(((pdpt[pdpte_idx]) & PTE_BASE_MASK) >> PTE_BASE_SHIFT));
    pde_idx = PD_INDEX(vpn);
    if (!(pd[pde_idx] & PTE_P)) return STATUS_PAGE_NOT_PRESENT;
    if ((pd[pde_idx] & PTE_PS)) {
        pfn = ((pd[pde_idx] & PTE_BASE_MASK) >> PTE_BASE_SHIFT) + (vpn & 0x1FF);
        contiguous_pages = ((St_PageCount)1 << 9) - (vpn & 0x1FF);
        goto has_span;
    }

    pt = PHYS_TO_VIRT(FRAME_TO_VPTR(((pd[pde_idx]) & PTE_BASE_MASK) >> PTE_BASE_SHIFT));
    pte_idx = PT_INDEX(vpn);
    if (!(pt[pte_idx] & PTE_P)) return STATUS_PAGE_NOT_PRESENT;

    pfn = (St_PhysFrame)((pt[pte_idx]) & PTE_BASE_MASK) >> PTE_BASE_SHIFT;
    contiguous_pages = 1;

    while (pte_idx + contiguous_pages < 512) {
        StA_PaePageTableEntry next_entry = pt[pte_idx + contiguous_pages];
        if (!(next_entry & PTE_P)) break;
        if ((St_PhysFrame)(next_entry & PTE_BASE_MASK) >> PTE_BASE_SHIFT != pfn + contiguous_pages)
            break;
        contiguous_pages++;
    }

has_span:
    available_len = ((size_t)contiguous_pages * PAGE_SIZE) - page_offset;
    if (available_len > max_len) {
        available_len = max_len;
    }

    *direct_ptr = (uint8_t *)PHYS_TO_VIRT(FRAME_TO_VPTR(pfn)) + page_offset;
    *span_len = available_len;

    return STATUS_SUCCESS;
}

StStatus StMmP_GlobalVirtPageToPhysFrame(St_VirtPage vpn __in, St_PhysFrame *pfn __out_optional)
{
    if (!IS_GLOBAL_VPN(vpn)) return STATUS_INVALID_VALUE;

    return vpn_to_pfn(&base_asp, vpn, pfn);
}

StStatus StMmP_LocalVirtPageToPhysFrame(
    StAddressSpace_StrongRef asp __in, St_VirtPage vpn __in, St_PhysFrame *pfn __out_optional
)
{
    if (!IS_LOCAL_VPN(vpn)) return STATUS_INVALID_VALUE;

    return vpn_to_pfn(asp, vpn, pfn);
}

StStatus StMmP_GetGlobalPageMapFlags(St_VirtPage vpn __in, StMm_MapFlags *map_flags __out)
{
    assert(map_flags);

    if (!IS_GLOBAL_VPN(vpn)) return STATUS_INVALID_VALUE;

    return vpn_to_page_map_flags(&base_asp, vpn, map_flags);
}

StStatus StMmP_GetLocalPageMapFlags(
    StAddressSpace_StrongRef asp __in, St_VirtPage vpn __in, StMm_MapFlags *map_flags __out
)
{
    assert(map_flags);

    if (!IS_LOCAL_VPN(vpn)) return STATUS_INVALID_VALUE;

    return vpn_to_page_map_flags(asp, vpn, map_flags);
}

static StStatus map_memory(
    StAddressSpace_StrongRef asp __in,
    St_PhysFrame pfn __in,
    St_VirtPage vpn __in,
    St_PageCount count __in,
    StMm_MapFlags mapflags __in
)
{
    StStatus status;
    StA_PageMapLevel4Entry *base_pml4 =
        PHYS_TO_VIRT(FRAME_TO_VPTR(base_asp.platform_data.root_table_pfn));
    StA_PageMapLevel4Entry *pml4;
    StA_PageDirPtrTableEntry *pdpt;
    StA_PaePageDirectoryEntry *pd;
    StA_PaePageTableEntry *pt;
    uint64_t pml4e_idx;
    uint64_t pdpte_idx;
    uint64_t pde_idx;
    uint64_t pte_idx;
    size_t chunk_size;
    St_PhysFrame alloc_pfn;
    StA_PaePageTableEntry pte_template;

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

    pte_template = 0;
    pte_template |= PTE_P;
    if (mapflags & MF_WRITABLE)
        pte_template |= PTE_RW;
    else
        pte_template &= ~PTE_RW;
    if (mapflags & MF_USER)
        pte_template |= PTE_US;
    else
        pte_template &= ~PTE_US;
    if (mapflags & MF_NO_CACHE)
        pte_template |= PTE_PCD;
    else
        pte_template &= ~PTE_PCD;
    if (mapflags & MF_WRITETHRU_CACHE)
        pte_template |= PTE_PWT;
    else
        pte_template &= ~PTE_PWT;
    pte_template |= mapflags_to_pte_private_bits(mapflags);
    if (g_p_cpu_features->has_nx) {
        if (mapflags & MF_NO_EXECUTE)
            pte_template |= PTE_XD;
        else
            pte_template &= ~PTE_XD;
    }

    if (vpn >= MEMMAP_GLOBAL_VPN_BASE) {
        StThread_LockPreemption();
    }

    pml4 = PHYS_TO_VIRT(FRAME_TO_VPTR(asp->platform_data.root_table_pfn));

    while (count > 0) {
        /* 1. PML4 */
        pml4e_idx = PML4_INDEX(vpn);
        if (!(pml4[pml4e_idx] & PTE_P)) {

            if (vpn >= MEMMAP_GLOBAL_VPN_BASE && !(pml4[pml4e_idx] & PTE_P) &&
                (base_pml4[pml4e_idx] & PTE_P)) {
                pml4[pml4e_idx] = base_pml4[pml4e_idx];

                pdpt = PHYS_TO_VIRT(
                    FRAME_TO_VPTR(((pml4[pml4e_idx]) & PTE_BASE_MASK) >> PTE_BASE_SHIFT)
                );
            } else {
                status = allocate_page_table_frame(&alloc_pfn, !(mapflags & MF_USER));
                if (!CHECK_SUCCESS(status)) goto has_error;
                pdpt = PHYS_TO_VIRT(FRAME_TO_VPTR(alloc_pfn));

                StA_PageMapLevel4Entry temp;

                temp = 0;
                temp = (temp & ~PTE_BASE_MASK) | ((uint64_t)alloc_pfn << PTE_BASE_SHIFT);
                temp |= PTE_P;
                temp |= PTE_RW;
                if (mapflags & MF_USER)
                    temp |= PTE_US;
                else
                    temp &= ~PTE_US;

                if (vpn >= MEMMAP_GLOBAL_VPN_BASE) {
                    if ((base_pml4[pml4e_idx] & PTE_P)) {
                        free_page_table_frame(alloc_pfn);

                        pml4[pml4e_idx] = base_pml4[pml4e_idx];

                        pdpt = PHYS_TO_VIRT(FRAME_TO_VPTR(
                            ((base_pml4[pml4e_idx]) & PTE_BASE_MASK) >> PTE_BASE_SHIFT
                        ));
                    } else {
                        base_pml4[pml4e_idx] = temp;
                        pml4[pml4e_idx] = temp;
                    }
                } else {
                    pml4[pml4e_idx] = temp;
                }
            }
        } else {
            if (mapflags & MF_USER) pml4[pml4e_idx] |= PTE_US;
            pdpt =
                PHYS_TO_VIRT(FRAME_TO_VPTR(((pml4[pml4e_idx]) & PTE_BASE_MASK) >> PTE_BASE_SHIFT));
        }

        /* 2. PDPT */
        pdpte_idx = PDPT_INDEX(vpn);
        if (!(pdpt[pdpte_idx] & PTE_P)) {
            status = allocate_page_table_frame(&alloc_pfn, !(mapflags & MF_USER));
            if (!CHECK_SUCCESS(status)) goto has_error;
            pd = PHYS_TO_VIRT(FRAME_TO_VPTR(alloc_pfn));

            pdpt[pdpte_idx] = 0;
            pdpt[pdpte_idx] =
                (pdpt[pdpte_idx] & ~PTE_BASE_MASK) | ((uint64_t)alloc_pfn << PTE_BASE_SHIFT);
            pdpt[pdpte_idx] |= PTE_P;
            pdpt[pdpte_idx] |= PTE_RW;
            if (mapflags & MF_USER)
                pdpt[pdpte_idx] |= PTE_US;
            else
                pdpt[pdpte_idx] &= ~PTE_US;
        } else {
            if ((pdpt[pdpte_idx] & PTE_PS)) {
                status = STATUS_CONFLICTING_STATE;
                goto has_error;
            }
            if (mapflags & MF_USER) pdpt[pdpte_idx] |= PTE_US;
            pd = PHYS_TO_VIRT(FRAME_TO_VPTR(((pdpt[pdpte_idx]) & PTE_BASE_MASK) >> PTE_BASE_SHIFT));
        }

        /* 3. PD */
        pde_idx = PD_INDEX(vpn);
        if (!(pd[pde_idx] & PTE_P)) {
            status = allocate_page_table_frame(&alloc_pfn, !(mapflags & MF_USER));
            if (!CHECK_SUCCESS(status)) goto has_error;
            pt = PHYS_TO_VIRT(FRAME_TO_VPTR(alloc_pfn));

            pd[pde_idx] = 0;
            pd[pde_idx] = (pd[pde_idx] & ~PTE_BASE_MASK) | ((uint64_t)alloc_pfn << PTE_BASE_SHIFT);
            pd[pde_idx] |= PTE_P;
            pd[pde_idx] |= PTE_RW;
            if (mapflags & MF_USER)
                pd[pde_idx] |= PTE_US;
            else
                pd[pde_idx] &= ~PTE_US;
        } else {
            if ((pd[pde_idx] & PTE_PS)) {
                status = STATUS_CONFLICTING_STATE;
                goto has_error;
            }
            if (mapflags & MF_USER) pd[pde_idx] |= PTE_US;
            pt = PHYS_TO_VIRT(FRAME_TO_VPTR(((pd[pde_idx]) & PTE_BASE_MASK) >> PTE_BASE_SHIFT));
        }

        /* 4. PT */
        pte_idx = PT_INDEX(vpn);

        chunk_size = 512 - pte_idx;
        if (count < chunk_size) {
            chunk_size = count;
        }

        for (size_t i = 0; i < chunk_size; i++) {
            if ((pt[pte_idx + i] & PTE_P)) {
                status = STATUS_DUPLICATE_ENTRY;
                goto has_error;
            }
            pt[pte_idx + i] = pte_template;
            pt[pte_idx + i] =
                (pt[pte_idx + i] & ~PTE_BASE_MASK) | (((uint64_t)pfn + i) << PTE_BASE_SHIFT);

            if (mapflags & MF_ZERO_FILL) {
                memset(
                    PHYS_TO_VIRT(FRAME_TO_VPTR(
                        (St_PhysFrame)((pt[pte_idx + i]) & PTE_BASE_MASK) >> PTE_BASE_SHIFT
                    )),
                    0,
                    PAGE_SIZE
                );
            }
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
    StAddressSpace_StrongRef asp __in,
    St_PhysFrame pfn __in,
    St_VirtPage vpn __in,
    St_PageCount count __in,
    StMm_MapFlags mapflags __in
)
{
    if (!IS_LOCAL_VPN(vpn)) return STATUS_INVALID_VALUE;

    return map_memory(asp, pfn, vpn, count, mapflags | MF_ZERO_FILL);
}

static StStatus set_managed_memory(
    StAddressSpace_StrongRef asp __in,
    St_VirtPage vpn __in,
    St_PageCount count __in,
    StMm_MapFlags mapflags __in
)
{
    StStatus status;
    StA_PageMapLevel4Entry *pml4;
    StA_PageDirPtrTableEntry *pdpt;
    StA_PaePageDirectoryEntry *pd;
    StA_PaePageTableEntry *pt;
    uint64_t pml4e_idx;
    uint64_t pdpte_idx;
    uint64_t pde_idx;
    uint64_t pte_idx;
    size_t chunk_size;
    St_PhysFrame alloc_pfn;

    if (count == 0) return STATUS_INVALID_VALUE;
    if (vpn > VIRT_PAGE_MAX) return STATUS_INVALID_VALUE;
    if ((St_VirtPage)(count - 1) > VIRT_PAGE_MAX - vpn) return STATUS_INVALID_VALUE;
    if (PML4_HOLE_START <= vpn && vpn <= PML4_HOLE_END) return STATUS_INVALID_VALUE;
    if (PML4_HOLE_START <= vpn + count - 1 && vpn + count - 1 <= PML4_HOLE_END) {
        return STATUS_INVALID_VALUE;
    }

    pml4 = PHYS_TO_VIRT(FRAME_TO_VPTR(asp->platform_data.root_table_pfn));

    while (count > 0) {
        /* 1. PML4 */
        pml4e_idx = PML4_INDEX(vpn);
        if (!(pml4[pml4e_idx] & PTE_P)) {
            status = allocate_page_table_frame(&alloc_pfn, !(mapflags & MF_USER));
            if (!CHECK_SUCCESS(status)) return status;
            pdpt = PHYS_TO_VIRT(FRAME_TO_VPTR(alloc_pfn));

            pml4[pml4e_idx] = 0;
            pml4[pml4e_idx] =
                (pml4[pml4e_idx] & ~PTE_BASE_MASK) | ((uint64_t)alloc_pfn << PTE_BASE_SHIFT);
            pml4[pml4e_idx] |= PTE_P;
            pml4[pml4e_idx] |= PTE_RW;
            if (mapflags & MF_USER) pml4[pml4e_idx] |= PTE_US;
        } else {
            if (mapflags & MF_USER) pml4[pml4e_idx] |= PTE_US;
            pdpt =
                PHYS_TO_VIRT(FRAME_TO_VPTR(((pml4[pml4e_idx]) & PTE_BASE_MASK) >> PTE_BASE_SHIFT));
        }

        /* 2. PDPT */
        pdpte_idx = PDPT_INDEX(vpn);
        if (!(pdpt[pdpte_idx] & PTE_P)) {
            status = allocate_page_table_frame(&alloc_pfn, !(mapflags & MF_USER));
            if (!CHECK_SUCCESS(status)) return status;
            pd = PHYS_TO_VIRT(FRAME_TO_VPTR(alloc_pfn));

            pdpt[pdpte_idx] = 0;
            pdpt[pdpte_idx] =
                (pdpt[pdpte_idx] & ~PTE_BASE_MASK) | ((uint64_t)alloc_pfn << PTE_BASE_SHIFT);
            pdpt[pdpte_idx] |= PTE_P;
            pdpt[pdpte_idx] |= PTE_RW;
            if (mapflags & MF_USER) pdpt[pdpte_idx] |= PTE_US;
        } else {
            if (pdpt[pdpte_idx] & PTE_PS) return STATUS_CONFLICTING_STATE;
            if (mapflags & MF_USER) pdpt[pdpte_idx] |= PTE_US;
            pd = PHYS_TO_VIRT(FRAME_TO_VPTR(((pdpt[pdpte_idx]) & PTE_BASE_MASK) >> PTE_BASE_SHIFT));
        }

        /* 3. PD */
        pde_idx = PD_INDEX(vpn);
        if (!(pd[pde_idx] & PTE_P)) {
            status = allocate_page_table_frame(&alloc_pfn, !(mapflags & MF_USER));
            if (!CHECK_SUCCESS(status)) return status;
            pt = PHYS_TO_VIRT(FRAME_TO_VPTR(alloc_pfn));

            pd[pde_idx] = 0;
            pd[pde_idx] = (pd[pde_idx] & ~PTE_BASE_MASK) | ((uint64_t)alloc_pfn << PTE_BASE_SHIFT);
            pd[pde_idx] |= PTE_P;
            pd[pde_idx] |= PTE_RW;
            if (mapflags & MF_USER) pd[pde_idx] |= PTE_US;
        } else {
            if (pd[pde_idx] & PTE_PS) return STATUS_CONFLICTING_STATE;
            if (mapflags & MF_USER) pd[pde_idx] |= PTE_US;
            pt = PHYS_TO_VIRT(FRAME_TO_VPTR(((pd[pde_idx]) & PTE_BASE_MASK) >> PTE_BASE_SHIFT));
        }

        /* 4. PT */
        pte_idx = PT_INDEX(vpn);
        chunk_size = 512 - pte_idx;
        if (count < chunk_size) {
            chunk_size = count;
        }

        for (size_t i = 0; i < chunk_size; i++) {
            if (pt[pte_idx + i] & PTE_P) return STATUS_DUPLICATE_ENTRY;
            if (pt[pte_idx + i] && !(pt[pte_idx + i] & PTE_MANAGED)) {
                return STATUS_CONFLICTING_STATE;
            }
            pt[pte_idx + i] = PTE_MANAGED;
        }

        count -= chunk_size;
        vpn += chunk_size;
    }

    return STATUS_SUCCESS;
}

static void clear_managed_memory(
    StAddressSpace_StrongRef asp __in, St_VirtPage vpn __in, St_PageCount count __in
)
{
    StA_PageMapLevel4Entry *pml4;

    if (!IS_LOCAL_VPN(vpn)) return;
    if (count == 0) return;

    pml4 = PHYS_TO_VIRT(FRAME_TO_VPTR(asp->platform_data.root_table_pfn));

    for (St_PageCount i = 0; i < count; i++) {
        St_VirtPage curr_vpn = vpn + i;
        uint64_t pml4e_idx;
        uint64_t pdpte_idx;
        uint64_t pde_idx;
        uint64_t pte_idx;
        StA_PageDirPtrTableEntry *pdpt;
        StA_PaePageDirectoryEntry *pd;
        StA_PaePageTableEntry *pt;

        if (curr_vpn > VIRT_PAGE_MAX) return;
        if (PML4_HOLE_START <= curr_vpn && curr_vpn <= PML4_HOLE_END) continue;

        pml4e_idx = PML4_INDEX(curr_vpn);
        if (!(pml4[pml4e_idx] & PTE_P)) continue;
        pdpt = PHYS_TO_VIRT(FRAME_TO_VPTR(((pml4[pml4e_idx]) & PTE_BASE_MASK) >> PTE_BASE_SHIFT));

        pdpte_idx = PDPT_INDEX(curr_vpn);
        if (!(pdpt[pdpte_idx] & PTE_P)) continue;
        if (pdpt[pdpte_idx] & PTE_PS) continue;
        pd = PHYS_TO_VIRT(FRAME_TO_VPTR(((pdpt[pdpte_idx]) & PTE_BASE_MASK) >> PTE_BASE_SHIFT));

        pde_idx = PD_INDEX(curr_vpn);
        if (!(pd[pde_idx] & PTE_P)) continue;
        if (pd[pde_idx] & PTE_PS) continue;
        pt = PHYS_TO_VIRT(FRAME_TO_VPTR(((pd[pde_idx]) & PTE_BASE_MASK) >> PTE_BASE_SHIFT));

        pte_idx = PT_INDEX(curr_vpn);
        if (!(pt[pte_idx] & PTE_P) && (pt[pte_idx] & PTE_MANAGED)) {
            pt[pte_idx] = 0;
            StA_InvalidatePage(curr_vpn);
        }
    }
}

StStatus StMmP_MapGlobalSparseMemory(
    St_VirtPage vpn __in, St_PageCount count __in, StMm_MapFlags mapflags __in
)
{
    (void)count;

    if (!IS_GLOBAL_VPN(vpn)) return STATUS_INVALID_VALUE;
    if (mapflags & MF_IMMEDIATE) return STATUS_SUCCESS;

    return STATUS_NOT_SUPPORTED;
}

StStatus StMmP_MapLocalSparseMemory(
    StAddressSpace_StrongRef asp __in,
    St_VirtPage vpn __in,
    St_PageCount count __in,
    StMm_MapFlags mapflags __in
)
{
    StStatus status;

    if (!IS_LOCAL_VPN(vpn)) return STATUS_INVALID_VALUE;
    if (mapflags & MF_IMMEDIATE) return STATUS_SUCCESS;

    status = set_managed_memory(asp, vpn, count, mapflags | MF_ZERO_FILL);
    if (!CHECK_SUCCESS(status)) {
        clear_managed_memory(asp, vpn, count);
    }

    return status;
}

static StStatus remap_memory(
    StAddressSpace_StrongRef asp __in,
    St_VirtPage vpn __in,
    St_PageCount count __in,
    StMm_MapFlags mapflags __in
)
{
    StA_PageMapLevel4Entry *pml4;
    StA_PageDirPtrTableEntry *pdpt;
    StA_PaePageDirectoryEntry *pd;
    StA_PaePageTableEntry *pt;
    uint64_t pml4e_idx;
    uint64_t pdpte_idx;
    uint64_t pde_idx;
    uint64_t pte_idx;
    size_t chunk_size;
    StA_PaePageTableEntry pte_template;
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

    pte_template = 0;
    pte_template |= PTE_P;
    if (mapflags & MF_WRITABLE)
        pte_template |= PTE_RW;
    else
        pte_template &= ~PTE_RW;
    if (mapflags & MF_USER)
        pte_template |= PTE_US;
    else
        pte_template &= ~PTE_US;
    if (mapflags & MF_NO_CACHE)
        pte_template |= PTE_PCD;
    else
        pte_template &= ~PTE_PCD;
    if (mapflags & MF_WRITETHRU_CACHE)
        pte_template |= PTE_PWT;
    else
        pte_template &= ~PTE_PWT;
    pte_template |= mapflags_to_pte_private_bits(mapflags);
    if (g_p_cpu_features->has_nx) {
        if (mapflags & MF_NO_EXECUTE)
            pte_template |= PTE_XD;
        else
            pte_template &= ~PTE_XD;
    }

    if (vpn >= MEMMAP_GLOBAL_VPN_BASE) {
        StThread_LockPreemption();
    }

    pml4 = PHYS_TO_VIRT(FRAME_TO_VPTR(asp->platform_data.root_table_pfn));

    while (count > 0) {
        /* 1. PML4 */
        pml4e_idx = PML4_INDEX(vpn);
        if (!(pml4[pml4e_idx] & PTE_P)) {
            return STATUS_PAGE_NOT_PRESENT;
        }

        if (mapflags & MF_USER) pml4[pml4e_idx] |= PTE_US;
        pdpt = PHYS_TO_VIRT(FRAME_TO_VPTR(((pml4[pml4e_idx]) & PTE_BASE_MASK) >> PTE_BASE_SHIFT));

        /* 2. PDPT */
        pdpte_idx = PDPT_INDEX(vpn);
        if (!(pdpt[pdpte_idx] & PTE_P)) {
            return STATUS_PAGE_NOT_PRESENT;
        }

        if (mapflags & MF_USER) pdpt[pdpte_idx] |= PTE_US;
        pd = PHYS_TO_VIRT(FRAME_TO_VPTR(((pdpt[pdpte_idx]) & PTE_BASE_MASK) >> PTE_BASE_SHIFT));

        /* 3. PD */
        pde_idx = PD_INDEX(vpn);
        if (!(pd[pde_idx] & PTE_P)) {
            return STATUS_PAGE_NOT_PRESENT;
        }

        if ((pd[pde_idx] & PTE_PS)) return STATUS_CONFLICTING_STATE;
        if (mapflags & MF_USER) pd[pde_idx] |= PTE_US;
        pt = PHYS_TO_VIRT(FRAME_TO_VPTR(((pd[pde_idx]) & PTE_BASE_MASK) >> PTE_BASE_SHIFT));

        /* 4. PT */
        pte_idx = vpn & VIRT_PAGE_PT_MASK;

        chunk_size = 512 - pte_idx;
        if (count < chunk_size) {
            chunk_size = count;
        }

        for (size_t i = 0; i < chunk_size; i++) {
            if (!(pt[pte_idx + i] & PTE_P)) {
                if (pt[pte_idx + i] & PTE_MANAGED) continue;
                return STATUS_PAGE_NOT_PRESENT;
            }
            pfn = ((pt[pte_idx + i]) & PTE_BASE_MASK) >> PTE_BASE_SHIFT;
            pt[pte_idx + i] = pte_template;
            pt[pte_idx + i] =
                (pt[pte_idx + i] & ~PTE_BASE_MASK) | ((uint64_t)pfn << PTE_BASE_SHIFT);

            if (do_invlpg) {
                StA_InvalidatePage(vpn + i);
            }
        }

        count -= chunk_size;
        vpn += chunk_size;
    }

    if (!do_invlpg) {
        StA_WriteCr3(StA_ReadCr3());
        release_quarantined_page_table_frames();
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
    StAddressSpace_StrongRef asp __in,
    St_VirtPage vpn __in,
    St_PageCount count __in,
    StMm_MapFlags mapflags __in
)
{
    if (!IS_LOCAL_VPN(vpn)) return STATUS_INVALID_VALUE;

    return remap_memory(asp, vpn, count, mapflags);
}

static void unmap_memory(
    StAddressSpace_StrongRef asp __in, St_VirtPage vpn __in, St_PageCount count __in
)
{
    StA_PageMapLevel4Entry *pml4;
    StA_PageDirPtrTableEntry *pdpt;
    StA_PaePageDirectoryEntry *pd;
    StA_PaePageTableEntry *pt;
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
        if (!(pml4[pml4e_idx] & PTE_P)) {
            St_Panic(STATUS_PAGE_NOT_PRESENT, "PML4 entry not present");
        }
        pdpt = PHYS_TO_VIRT(FRAME_TO_VPTR(((pml4[pml4e_idx]) & PTE_BASE_MASK) >> PTE_BASE_SHIFT));

        /* 2. PDPT */
        pdpte_idx = PDPT_INDEX(vpn);
        if (!(pdpt[pdpte_idx] & PTE_P)) {
            St_Panic(STATUS_PAGE_NOT_PRESENT, "PDPT entry not present");
        }
        pd = PHYS_TO_VIRT(FRAME_TO_VPTR(((pdpt[pdpte_idx]) & PTE_BASE_MASK) >> PTE_BASE_SHIFT));

        /* 3. PD */
        pde_idx = PD_INDEX(vpn);
        if (!(pd[pde_idx] & PTE_P)) {
            St_Panic(STATUS_PAGE_NOT_PRESENT, "PD entry not present");
        }
        pt = PHYS_TO_VIRT(FRAME_TO_VPTR(((pd[pde_idx]) & PTE_BASE_MASK) >> PTE_BASE_SHIFT));

        /* 4. PT */
        pte_idx = vpn & VIRT_PAGE_PT_MASK;

        chunk_size = 512 - pte_idx;
        if (count < chunk_size) {
            chunk_size = count;
        }

        for (size_t i = 0; i < chunk_size; i++) {
            if (!(pt[pte_idx + i] & PTE_P)) {
                St_Panic(STATUS_PAGE_NOT_PRESENT, "UnmapMemory: PT entry not present");
            }
            pt[pte_idx + i] = 0;

            if (do_invlpg) {
                StA_InvalidatePage(vpn + i);
            }
        }

        count -= chunk_size;
        vpn += chunk_size;
    }

    if (!do_invlpg) {
        StA_WriteCr3(StA_ReadCr3());
        release_quarantined_page_table_frames();
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
    StAddressSpace_StrongRef asp __in, St_VirtPage vpn __in, St_PageCount count __in
)
{
    if (!IS_LOCAL_VPN(vpn)) return;

    unmap_memory(asp, vpn, count);
}

void StMmP_UnmapGlobalSparseMemory(St_VirtPage vpn __in, St_PageCount count __in)
{
    if (!IS_GLOBAL_VPN(vpn)) return;

    clear_managed_memory(&base_asp, vpn, count);
}

void StMmP_UnmapLocalSparseMemory(
    StAddressSpace_StrongRef asp __in, St_VirtPage vpn __in, St_PageCount count __in
)
{
    if (!IS_LOCAL_VPN(vpn)) return;

    clear_managed_memory(asp, vpn, count);
}

StStatus StMmP_ReadLocal(
    StAddressSpace_StrongRef asp __in, uintptr_t addr __in, void *buf __buf, size_t len __in
)
{
    StStatus status;
    uint8_t *bbuf = buf;
    uint8_t *src;
    size_t copy_size;

    while (len > 0) {
        status = local_addr_to_directmap_span(asp, addr, len, &src, &copy_size);
        if (status == STATUS_PAGE_NOT_PRESENT) {
            status = StMm_HandlePageFault(asp, addr, 0);
            if (CHECK_SUCCESS(status)) {
                status = local_addr_to_directmap_span(asp, addr, len, &src, &copy_size);
            }
        }
        if (!CHECK_SUCCESS(status)) return status;

        memcpy(bbuf, src, copy_size);

        addr += copy_size;
        bbuf += copy_size;
        len -= copy_size;
    }

    return STATUS_SUCCESS;
}

StStatus StMmP_WriteLocal(
    StAddressSpace_StrongRef asp __in, uintptr_t addr __in, const void *buf __in, size_t len __in
)
{
    StStatus status;
    const uint8_t *bbuf = buf;
    uint8_t *dest;
    size_t copy_size;

    while (len > 0) {
        status = local_addr_to_directmap_span(asp, addr, len, &dest, &copy_size);
        if (status == STATUS_PAGE_NOT_PRESENT) {
            status = StMm_HandlePageFault(asp, addr, 0);
            if (CHECK_SUCCESS(status)) {
                status = local_addr_to_directmap_span(asp, addr, len, &dest, &copy_size);
            }
        }
        if (!CHECK_SUCCESS(status)) return status;

        memcpy(dest, bbuf, copy_size);

        addr += copy_size;
        bbuf += copy_size;
        len -= copy_size;
    }

    return STATUS_SUCCESS;
}

StStatus StMmP_SetLocal(
    StAddressSpace_StrongRef asp __in, uintptr_t addr __in, int value, size_t len __in
)
{
    StStatus status;
    uint8_t *dest;
    size_t copy_size;

    while (len > 0) {
        status = local_addr_to_directmap_span(asp, addr, len, &dest, &copy_size);
        if (status == STATUS_PAGE_NOT_PRESENT) {
            status = StMm_HandlePageFault(asp, addr, 0);
            if (CHECK_SUCCESS(status)) {
                status = local_addr_to_directmap_span(asp, addr, len, &dest, &copy_size);
            }
        }
        if (!CHECK_SUCCESS(status)) return status;

        memset(dest, value, copy_size);

        addr += copy_size;
        len -= copy_size;
    }

    return STATUS_SUCCESS;
}

StStatus StMmP_CopyLocal(
    StAddressSpace_StrongRef dest_asp __in,
    uintptr_t dest __in,
    StAddressSpace_StrongRef src_asp __in,
    uintptr_t src __in,
    size_t len __in
)
{
    // TODO: implement
    return STATUS_NOT_IMPLEMENTED;
}

StStatus StMmP_MapConventionalMemory(St_VirtPage *vpn __out)
{
    assert(vpn);

    static int mapped = 0;
    static St_VirtPage mapped_vpn;

    if (!mapped) {
        StStatus status;

        status = StMm_MapGlobal(
            VMM_DOMAIN_IO,
            &mapped_vpn,
            0,
            256,
            NULL,
            (struct StMm_CompoundFlags){AF_DEFAULT, MF_KERNEL_DEFAULT}
        );
        if (!CHECK_SUCCESS(status)) return status;

        mapped = 1;
    }

    *vpn = mapped_vpn;

    return STATUS_SUCCESS;
}
