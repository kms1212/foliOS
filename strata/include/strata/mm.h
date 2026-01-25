#ifndef __STRATA_MM_H__
#define __STRATA_MM_H__

#include <stddef.h>
#include <stdint.h>

#include <strata/arch/mmu.h>

#include <loadst/bootinfo.h>
#include <strata/compiler.h>
#include <strata/status.h>
#include <strata/types.h>

#define FRAME_TO_UINT(f) ((uintptr_t)(f))
#define UINT_TO_FRAME(u) ((St_PhysFrame)(u))

#define FRAME_TO_ADDR(f) ((uintptr_t)(f) * PAGE_SIZE)
#define ADDR_TO_FRAME(a) ((St_PhysFrame)((a) / PAGE_SIZE))
#define FRAME_TO_VPTR(f) ((void *)((uintptr_t)(f) * PAGE_SIZE))
#define VPTR_TO_FRAME(v) ((St_PhysFrame)((uintptr_t)(v) / PAGE_SIZE))

#define PAGE_TO_UINT(p) ((uintptr_t)(p))
#define UINT_TO_PAGE(u) ((St_VirtPage)(u))

#define PAGE_TO_ADDR(p) ((uintptr_t)(p) * PAGE_SIZE)
#define ADDR_TO_PAGE(a) ((St_VirtPage)((a) / PAGE_SIZE))
#define PAGE_TO_VPTR(p) ((void *)((uintptr_t)(p) * PAGE_SIZE))
#define VPTR_TO_PAGE(v) ((St_VirtPage)((uintptr_t)(v) / PAGE_SIZE))

typedef uint32_t StMm_MapFlags __nocast;
typedef uint32_t StPmm_AllocFlags __nocast;
typedef uint32_t StVmm_AllocFlags __nocast;

#define PMM_DEFAULT ((StPmm_AllocFlags)0x00000000)

#define PMM_BELOW_MASK ((StPmm_AllocFlags)0x0000000F)
#define PMM_BELOW_NONE ((StPmm_AllocFlags)0x00000000)
#define PMM_BELOW_1M   ((StPmm_AllocFlags)0x00000001)
#define PMM_BELOW_16M  ((StPmm_AllocFlags)0x00000002)
#define PMM_BELOW_4G   ((StPmm_AllocFlags)0x00000003)

#define PMM_ALIGN_MASK ((StPmm_AllocFlags)0x000003F0)
#define PMM_ALIGN_AUTO ((StPmm_AllocFlags)0x00000000)
#define PMM_ALIGN(a)                                                                               \
    ((StPmm_AllocFlags)(((uint32_t)(a < 1 ? 0 : __builtin_ctzll(a)) << 4) & PMM_ALIGN_MASK))

#define VMM_DEFAULT ((StVmm_AllocFlags)0x00000000)

#define MAP_DEFAULT         ((StMm_MapFlags)0x00000000)
#define MAP_READONLY        ((StMm_MapFlags)0x00000001)
#define MAP_USER            ((StMm_MapFlags)0x00000002)
#define MAP_NO_CACHE        ((StMm_MapFlags)0x00000004)
#define MAP_WRITETHRU_CACHE ((StMm_MapFlags)0x00000008)
#define MAP_NO_EXECUTE      ((StMm_MapFlags)0x00000010)

enum StVmm_Domain {
    VMM_DOMAIN_KERNEL_FAST = 0,
    VMM_DOMAIN_KERNEL_SLOW,
    VMM_DOMAIN_IO,
    VMM_DOMAIN_MODULE,
    VMM_DOMAIN_USER,
    VMM_DOMAIN_MAX,
};

struct StPmm_AllocationMetadata {
    const St_PhysFrame pfn;
    void *const owner;
    uint32_t flags;
    const uint32_t order;
};

StStatus StPmm_Init(void);
StStatus StPmm_LateInit(void);

StStatus StPmm_GetTotalFrameCount(St_PageCount *frame_count __out);
StStatus StPmm_GetFreeFrameCount(St_PageCount *count __out);

StStatus StPmm_AllocateContiguousFrame(
    St_PhysFrame *pfn __out, St_PageCount count __in, StPmm_AllocFlags alloc_flags __in
);
StStatus StPmm_AcquireContiguousFrame(St_PhysFrame pfn __in);
void StPmm_FreeContiguousFrame(St_PhysFrame pfn __in);

StStatus StPmm_MarkUsableContiguousFrame(St_PhysFrame base __in, St_PhysFrame limit __in);
StStatus StPmm_MarkUnusableContiguousFrame(St_PhysFrame base __in, St_PhysFrame limit __in);

StStatus StPmm_GetAllocMetadata(
    St_PhysFrame pfn __in, struct StPmm_AllocationMetadata **meta __out
);
StStatus StPmm_LockAndGetAllocMetadata(
    St_PhysFrame pfn, struct StPmm_AllocationMetadata **meta __out
);
StStatus StPmm_UnlockAllocMetadata(struct StPmm_AllocationMetadata *meta __in);

#ifdef TESTING
void StPmm_DebugDumpRegion(St_PhysFrame start_pfn __in, St_PageCount count __in);
void StPmm_DebugDumpAtpa(void);
#endif

StStatus StVmm_InitDomain(
    enum StVmm_Domain domain __in, St_VirtPage base_vpn __in, St_VirtPage limit_vpn __in
);

StStatus StVmm_GetTotalPageCount(enum StVmm_Domain domain __in, St_PageCount *count __out);
StStatus StVmm_GetFreePageCount(enum StVmm_Domain domain, St_PageCount *count __out);

StStatus StVmm_AllocatePage(
    enum StVmm_Domain domain __in,
    St_VirtPage *vpn __out,
    St_PageCount count __in,
    StVmm_AllocFlags alloc_flags __in
);
void StVmm_FreePage(St_VirtPage vpn __in, St_PageCount count __in);

StStatus StMm_Init(void);

StStatus StMm_VirtPageToPhysFrame(St_VirtPage vpn __in, St_PhysFrame *pfn __out_optional);
__always_inline StStatus
StMm_VirtAddrToPhysAddr(uintptr_t vaddr __in, uintptr_t *paddr __out_optional)
{
    StStatus status;
    St_PhysFrame pfn;

    status = StMm_VirtPageToPhysFrame(ADDR_TO_PAGE(vaddr), &pfn);
    if (!CHECK_SUCCESS(status)) return status;

    *paddr = PAGE_TO_ADDR(pfn) + (vaddr % PAGE_SIZE);

    return STATUS_SUCCESS;
}

StStatus StMm_MapContiguous(
    enum StVmm_Domain domain __in,
    St_VirtPage *vpn __out,
    St_PhysFrame pfn __in,
    St_PageCount count __in,
    StVmm_AllocFlags vmmflags __in,
    StMm_MapFlags mapflags __in
);
void StMm_UnmapContiguous(St_VirtPage vpn __in, St_PageCount count __in);

StStatus StMm_AllocateContiguous(
    enum StVmm_Domain domain __in,
    St_VirtPage *vpn __out,
    St_PageCount count __in,
    StPmm_AllocFlags pmmflags __in,
    StVmm_AllocFlags vmmflags __in,
    StMm_MapFlags mapflags __in
);
StStatus StMm_AllocateSparse(
    enum StVmm_Domain domain __in,
    St_VirtPage *vpn __out,
    St_PageCount count __in,
    StPmm_AllocFlags pmmflags __in,
    StVmm_AllocFlags vmmflags __in,
    StMm_MapFlags mapflags __in
);
void StMm_Free(St_VirtPage vpn __in, St_PageCount count __in);

StStatus StMm_SetPageFlags(
    St_VirtPage vpn __in, St_PageCount count __in, StMm_MapFlags mapflags __in
);
StStatus StMm_GetPageFlags(St_VirtPage vpn __in, StMm_MapFlags *mapflags __out);

#endif  // __STRATA_MM_H__
