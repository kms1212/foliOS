#include <vellum/mm.h>

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <vellum/arch/cpufeatures.h>
#include <vellum/arch/intrinsics/invlpg.h>
#include <vellum/arch/intrinsics/register.h>

#include <vellum/plat/instruction.h>
#include <vellum/plat/page.h>

#include <vellum/log.h>
#include <vellum/macros.h>
#include <vellum/panic.h>

#define MODULE_NAME "mm"

extern int __end;

#define PBM_GET(idx) ((pma_bitmap[(idx) >> 2] >> (((idx) & 3) * 2)) & 3)
#define PBM_SET(idx, val)                                                                          \
    do {                                                                                           \
        pma_bitmap[(idx) >> 2] &= ~(3 << (((idx) & 3) * 2));                                       \
        pma_bitmap[(idx) >> 2] |= (val) << (((idx) & 3) * 2);                                      \
    } while (0)

#define PBM_FREE      0
#define PBM_ALLOCATED 1
#define PBM_RESERVED  2

static uint8_t *pma_bitmap;
static size_t pma_bitmap_size;
static size_t pma_frame_desc_count;
static size_t pma_available_frames, pma_free_frames;
static pfn_t pma_base_pfn, pma_limit_pfn;

struct page_dir_recursive page_dir_recursive __aligned(PAGE_SIZE);

struct page_dir_recursive *_pc_page_dir;

status_t mm_pma_init(uintptr_t base_paddr, uintptr_t limit_paddr)
{
    status_t status;

    pma_bitmap = (void *)ALIGN((uintptr_t)&__end, PAGE_SIZE);

    pma_frame_desc_count = limit_paddr / PAGE_SIZE - base_paddr / PAGE_SIZE;
    pma_available_frames = pma_frame_desc_count;
    pma_free_frames = pma_frame_desc_count;

    /* 2 bits per frame descriptor -> 4 frame descriptors per byte */
    pma_bitmap_size = (pma_frame_desc_count + 3) / 4;

    pma_base_pfn = base_paddr / PAGE_SIZE;
    pma_limit_pfn = limit_paddr / PAGE_SIZE;

    memset(pma_bitmap, 0, pma_bitmap_size);

    LOG_DEBUG(
        "PMA bitmap initialized to 0x%p. basepfn=%zu limitpfn=%zu\n",
        pma_bitmap,
        pma_base_pfn,
        pma_limit_pfn
    );

    status = mm_pma_mark_reserved(base_paddr, (uintptr_t)pma_bitmap + pma_bitmap_size - 1);
    if (!CHECK_SUCCESS(status)) return status;

    return STATUS_SUCCESS;
}

status_t mm_pma_mark_reserved(uintptr_t base_paddr, uintptr_t limit_paddr)
{
    uintptr_t base_page, limit_page;

    base_page = base_paddr / PAGE_SIZE;
    limit_page = limit_paddr / PAGE_SIZE;

    if (base_page < pma_base_pfn) {
        base_page = pma_base_pfn;
    }

    if (limit_page > pma_limit_pfn) {
        limit_page = pma_limit_pfn;
    }

    for (size_t i = base_page - pma_base_pfn; i <= limit_page - pma_base_pfn; i++) {
        if (PBM_GET(i) == PBM_RESERVED) continue;

        if (PBM_GET(i) == PBM_FREE) {
            pma_free_frames--;
        }

        pma_available_frames--;

        PBM_SET(i, PBM_RESERVED);
    }

    LOG_TRACE("marked frame %08zX-%08zX to reserved\n", base_page, limit_page);

    return STATUS_SUCCESS;
}

status_t mm_pma_get_available_frame_count(size_t *frame_count)
{
    if (frame_count) *frame_count = pma_available_frames;

    return STATUS_SUCCESS;
}

status_t mm_pma_get_free_frame_count(size_t *frame_count)
{
    if (frame_count) *frame_count = pma_free_frames;

    return STATUS_SUCCESS;
}

status_t mm_pma_allocate_frame(size_t count, pfn_t *pfn)
{
    size_t free_count = 0;
    uintptr_t alloc_start_idx = 0;

    for (size_t i = 0; i < pma_frame_desc_count; i++) {
        if (PBM_GET(i) != PBM_FREE) {
            free_count = 0;
            continue;
        }

        if (free_count == 0) {
            alloc_start_idx = i;
        }

        free_count++;

        if (free_count >= count) {
            break;
        }
    }

    if (free_count < count) {
        return STATUS_INSUFFICIENT_MEMORY;
    }

    for (size_t i = alloc_start_idx; i < alloc_start_idx + count; i++) {
        PBM_SET(i, PBM_ALLOCATED);
        pma_free_frames--;
    }

    if (pfn) *pfn = pma_base_pfn + alloc_start_idx;

    LOG_TRACE(
        "allocated frame %zu-%zu\n",
        pma_base_pfn + alloc_start_idx,
        pma_base_pfn + alloc_start_idx + count - 1
    );

    return STATUS_SUCCESS;
}

void mm_pma_free_frame(pfn_t pfn, size_t frame_count)
{
    if (pfn < pma_base_pfn || pfn > pma_limit_pfn) return;

    if (pfn + frame_count > pma_limit_pfn + 1) {
        frame_count -= pma_limit_pfn + 1 - pfn;
    }

    uintptr_t free_start_idx = pfn - pma_base_pfn;

    for (size_t i = free_start_idx; i < free_start_idx + frame_count; i++) {
        if (PBM_GET(i) != PBM_ALLOCATED) continue;

        PBM_SET(i, PBM_FREE);
        pma_free_frames++;
    }

    LOG_TRACE(
        "freed frame %08zX-%08zX\n",
        pma_base_pfn + free_start_idx,
        pma_base_pfn + free_start_idx + frame_count - 1
    );
}

static vpn_t alloc_virt_page(size_t page_count)
{
    static vpn_t alloc_vpn = 0x00000400;
    vpn_t new_vpn = alloc_vpn;

    alloc_vpn += page_count;

    return new_vpn;
}

static status_t init_page_directory(void)
{
    status_t status;
    pfn_t new_pt_pfn;
    union VlA_PageTableEntry *pt;

    for (int i = 1; i < 1023; i++) {
        page_dir_recursive.pde[i].raw = 0x00000002;
    }

    page_dir_recursive.pte.raw = 0x00000003 | ((uintptr_t)&page_dir_recursive & 0xFFFFF000);

    status = mm_pma_allocate_frame(1, &new_pt_pfn);
    if (!CHECK_SUCCESS(status)) return status;

    page_dir_recursive.pde[0].raw = 0x00000003 | (new_pt_pfn << 12);

    pt = (void *)(new_pt_pfn * PAGE_SIZE);
    for (int i = 0; i < 1024; i++) {
        pt[i].raw = 0x00000003 | (i << 12);
    }

    _pc_page_dir = (void *)0xFFFFF000;

    return STATUS_SUCCESS;
}

status_t mm_init(void)
{
    status_t status;
    uint32_t cr0;

    LOG_DEBUG("initializing page directory...\n");
    status = init_page_directory();
    if (!CHECK_SUCCESS(status)) return status;

    LOG_DEBUG("setting up registers...\n");
    VlA_WriteCr3((uintptr_t)&page_dir_recursive);
    cr0 = VlA_ReadCr0();
    cr0 |= CR0_PG;
    VlA_WriteCr0(cr0);

    return STATUS_SUCCESS;
}

status_t mm_vpn_to_pfn(vpn_t vpn, pfn_t *pfn)
{
    union VlA_PageTableEntry *pt =
        (void *)0xFFC00000;  // NOLINT(clang-analyzer-core.FixedAddressDereference)

    if (vpn > 0x000FFFFF) return STATUS_INVALID_VALUE;

    if (!_pc_page_dir->pde[(vpn & 0x000FFC00) >> 10].dir.p) return STATUS_PAGE_NOT_PRESENT;
    if (!pt[vpn].p) return STATUS_PAGE_NOT_PRESENT;

    if (pfn) *pfn = pt[vpn].base;

    return STATUS_SUCCESS;
}

status_t mm_vaddr_to_paddr(void *vaddr, uintptr_t *paddr)
{
    status_t status;
    pfn_t pfn;

    status = mm_vpn_to_pfn((uintptr_t)vaddr >> 12, &pfn);
    if (!CHECK_SUCCESS(status)) return status;

    if (paddr) *paddr = pfn * PAGE_SIZE + (((uintptr_t)vaddr) & (PAGE_SIZE - 1));

    return STATUS_SUCCESS;
}

static void invalidate_page(vpn_t vpn)
{
    if (g_p_cpu_features->has_invlpg) {
        VlA_Invlpg((void *)(vpn * PAGE_SIZE));
    } else {
        VlA_WriteCr3(VlA_ReadCr3());
    }
}

static status_t map(pfn_t pfn, vpn_t vpn, uint32_t flags)
{
    status_t status;
    union VlA_PageTableEntry *pt = (void *)0xFFC00000;
    pfn_t new_pt_pfn;

    if (vpn > 0x000FFFFF) return STATUS_INVALID_VALUE;

    if (!_pc_page_dir->pde[(vpn & 0x000FFC00) >> 10].dir.p) {
        // create a new page table
        status = mm_pma_allocate_frame(1, &new_pt_pfn);
        if (!CHECK_SUCCESS(status)) return status;

        _pc_page_dir->pde[(vpn & 0x000FFC00) >> 10].raw = 0x00000003 | (new_pt_pfn << 12);

        invalidate_page((uintptr_t)&pt[vpn & 0xFFC00] / PAGE_SIZE);

        for (int i = 0; i < 1024; i++) {
            pt[(vpn & 0xFFC00) + i].raw = 0x00000000;
        }
    }

    if (pt[vpn].p) {
        if (!!(flags & PF_READONLY) != !pt[vpn].r_w) {
            return STATUS_CONFLICTING_STATE;
        }

        if (!(flags & PF_USER) != !pt[vpn].u_s) {
            return STATUS_CONFLICTING_STATE;
        }

        if (!(flags & PF_NOCACHE) != !(pt[vpn].pat && pt[vpn].pcd)) {
            return STATUS_CONFLICTING_STATE;
        }

        if (!(flags & PF_WTCACHE) != !(pt[vpn].pat && pt[vpn].pwt)) {
            return STATUS_CONFLICTING_STATE;
        }
    }

    pt[vpn].raw = 0;
    pt[vpn].base = pfn;

    if (!(flags & PF_READONLY)) {
        pt[vpn].r_w = 1;
    }

    if (flags & PF_USER) {
        pt[vpn].u_s = 1;
    }

    if ((flags & PF_NOCACHE) || (flags & PF_WTCACHE)) {
        pt[vpn].pat = 1;

        if (flags & PF_NOCACHE) {
            pt[vpn].pcd = 1;
        }

        if (flags & PF_WTCACHE) {
            pt[vpn].pwt = 1;
        }
    }

    pt[vpn].p = 1;

    invalidate_page(vpn);

    return STATUS_SUCCESS;
}

status_t mm_map(pfn_t pfn, vpn_t vpn, size_t page_count, uint32_t flags)
{
    status_t status;

    for (size_t i = 0; i < page_count; i++) {
        status = map(pfn + i, vpn + i, flags);
        if (!CHECK_SUCCESS(status)) return status;
    }

    // TODO: rollback if failed

    LOG_TRACE(
        "mapped page %zu-%zu to frame %zu-%zu\n",
        pfn,
        pfn + page_count - 1,
        vpn,
        vpn + page_count - 1
    );

    return STATUS_SUCCESS;
}

static void unmap(vpn_t vpn)
{
    union VlA_PageTableEntry *pt =
        (void *)0xFFC00000;  // NOLINT(clang-analyzer-core.FixedAddressDereference)

    if (vpn > 0x000FFFFF) return;

    if (!_pc_page_dir->pde[(vpn & 0x000FFC00) >> 10].dir.p) return;
    if (!pt[vpn].p) return;

    pt[vpn].raw = 0;

    invalidate_page(vpn);
}

status_t mm_unmap(vpn_t vpn, size_t page_count)
{
    LOG_TRACE("unmapping page %zu-%zu\n", vpn, vpn + page_count - 1);

    for (size_t i = 0; i < page_count; i++) {
        unmap(vpn + i);
    }

    return STATUS_SUCCESS;
}

status_t mm_allocate_pages(size_t page_count, vpn_t *vpn)
{
    status_t status;
    pfn_t allocated_pfn;
    vpn_t allocated_vpn;

    allocated_vpn = alloc_virt_page(page_count);

    for (size_t i = 0; i < page_count; i++) {
        status = mm_vpn_to_pfn(allocated_vpn + i, NULL);
        if (!CHECK_SUCCESS(status) && status != STATUS_PAGE_NOT_PRESENT) return status;

        if (status == STATUS_PAGE_NOT_PRESENT) {
            status = mm_pma_allocate_frame(1, &allocated_pfn);
            if (!CHECK_SUCCESS(status)) return status;

            status = mm_map(allocated_pfn, allocated_vpn + i, 1, PF_DEFAULT);
            if (!CHECK_SUCCESS(status)) return status;
        }
    }

    if (vpn) *vpn = allocated_vpn;

    return STATUS_SUCCESS;
}

status_t mm_allocate_pages_to(vpn_t vpn, size_t page_count)
{
    status_t status;
    pfn_t allocated_pfn;

    for (size_t i = 0; i < page_count; i++) {
        status = mm_vpn_to_pfn(vpn + i, NULL);
        if (!CHECK_SUCCESS(status) && status != STATUS_PAGE_NOT_PRESENT) return status;

        if (status == STATUS_PAGE_NOT_PRESENT) {
            status = mm_pma_allocate_frame(1, &allocated_pfn);
            if (!CHECK_SUCCESS(status)) return status;

            status = mm_map(allocated_pfn, vpn + i, 1, PF_DEFAULT);
            if (!CHECK_SUCCESS(status)) return status;
        }
    }

    return STATUS_SUCCESS;
}

void mm_free_pages(vpn_t vpn, size_t page_count)
{
    status_t status;
    pfn_t pfn;

    for (size_t i = 0; i < page_count; i++) {
        status = mm_vpn_to_pfn(vpn + i, &pfn);
        if (!CHECK_SUCCESS(status)) continue;

        status = mm_unmap(vpn + i, 1);
        if (!CHECK_SUCCESS(status)) continue;

        mm_pma_free_frame(pfn, 1);
    }
}
