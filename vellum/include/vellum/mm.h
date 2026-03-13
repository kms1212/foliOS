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

typedef uintptr_t pfn_t;
typedef uintptr_t vpn_t;

status_t mm_pma_init(uintptr_t base_paddr, uintptr_t limit_paddr);
status_t mm_pma_mark_reserved(uintptr_t base_paddr, uintptr_t limit_paddr);

status_t mm_pma_get_available_frame_count(size_t *frame_count);
status_t mm_pma_get_free_frame_count(size_t *frame_count);

status_t mm_pma_allocate_frame(size_t frame_count, pfn_t *pfn);
void mm_pma_free_frame(pfn_t pfn, size_t frame_count);

status_t mm_init(void);

status_t mm_vpn_to_pfn(vpn_t vpn, pfn_t *pfn);
status_t mm_vaddr_to_paddr(void *vaddr, uintptr_t *paddr);

status_t mm_map(pfn_t pfn, vpn_t vpn, size_t page_count, uint32_t flags);
status_t mm_unmap(vpn_t vpn, size_t page_count);

status_t mm_allocate_pages(size_t page_count, vpn_t *vpn);
status_t mm_allocate_pages_to(vpn_t vpn, size_t page_count);
void mm_free_pages(vpn_t vpn, size_t page_count);

#endif  // __VELLUM_MM_H__
