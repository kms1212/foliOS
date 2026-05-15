#ifndef __VELLUM_MM_H__
#define __VELLUM_MM_H__

#include <stddef.h>
#include <stdint.h>

#include <vellum/plat/page.h>

#include <vellum/status.h>

#define PF_DEFAULT  0x00000000
#define PF_READONLY 0x00000001
#define PF_USER     0x00000002
#define PF_NOCACHE  0x00000004
#define PF_WTCACHE  0x00000008

/**
 * Physical frame number in Vellum's loader address space.
 */
typedef uintptr_t pfn_t;

/**
 * Virtual page number in Vellum's loader address space.
 */
typedef uintptr_t vpn_t;

/**
 * Initializes the physical memory allocator over the supplied physical range.
 *
 * `base_paddr` and `limit_paddr` are inclusive byte addresses. The range is
 * rounded by the implementation to page-frame boundaries.
 */
VlStatus mm_pma_init(uintptr_t base_paddr, uintptr_t limit_paddr);

/**
 * Marks the inclusive physical byte range [base_paddr, limit_paddr]
 * unavailable for later frame allocation.
 */
VlStatus mm_pma_mark_reserved(uintptr_t base_paddr, uintptr_t limit_paddr);

/**
 * Returns the total frame count tracked by the physical allocator.
 */
VlStatus mm_pma_get_available_frame_count(size_t *frame_count);

/**
 * Returns the currently free frame count tracked by the physical allocator.
 */
VlStatus mm_pma_get_free_frame_count(size_t *frame_count);

/**
 * Allocates a physically contiguous run of frames.
 */
VlStatus mm_pma_allocate_frame(size_t frame_count, pfn_t *pfn);

/**
 * Frees a physically contiguous frame run previously allocated by the PMA.
 */
void mm_pma_free_frame(pfn_t pfn, size_t frame_count);

/**
 * Initializes Vellum's virtual memory layer for the active platform.
 */
VlStatus mm_init(void);

/**
 * Translates a mapped virtual page to its physical frame.
 */
VlStatus mm_vpn_to_pfn(vpn_t vpn, pfn_t *pfn);

/**
 * Translates a mapped virtual address to a physical address.
 */
VlStatus mm_vaddr_to_paddr(void *vaddr, uintptr_t *paddr);

/**
 * Maps an existing physical frame run at the requested virtual page.
 *
 * The caller owns the frames. The mapping flags use the PF_* domain above.
 */
VlStatus mm_map(pfn_t pfn, vpn_t vpn, size_t page_count, uint32_t flags);

/**
 * Removes virtual mappings without freeing the underlying physical frames.
 */
VlStatus mm_unmap(vpn_t vpn, size_t page_count);

/**
 * Allocates virtual pages backed by newly allocated frames.
 */
VlStatus mm_allocate_pages(size_t page_count, vpn_t *vpn);

/**
 * Allocates backing frames and maps them at an exact virtual page.
 */
VlStatus mm_allocate_pages_to(vpn_t vpn, size_t page_count);

/**
 * Unmaps virtual pages and releases their backing frames.
 */
void mm_free_pages(vpn_t vpn, size_t page_count);

#endif  // __VELLUM_MM_H__
