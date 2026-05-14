#ifndef __STRATA_MM_TYPES_H__
#define __STRATA_MM_TYPES_H__

#include <stddef.h>
#include <stdint.h>

#include <strata/arch/mmu_constants.h>

#include <strata/compiler.h>

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

#define AF_PMM_BELOW_MASK ((StMm_AllocFlags)0x00000007)
#define AF_PMM_BELOW_NONE ((StMm_AllocFlags)0x00000000)
#define AF_PMM_BELOW_1M   ((StMm_AllocFlags)0x00000001)
#define AF_PMM_BELOW_16M  ((StMm_AllocFlags)0x00000002)
#define AF_PMM_BELOW_4G   ((StMm_AllocFlags)0x00000003)

#define AF_VMM_GUARD    ((StMm_AllocFlags)0x00010000)
#define AF_VMM_TOP_DOWN ((StMm_AllocFlags)0x00020000)

#define AF_ALIGN_MASK ((StMm_AllocFlags)0x000003F0)
#define AF_ALIGN_AUTO ((StMm_AllocFlags)0x00000000)
#define AF_ALIGN(a)                                                                                \
    ((StMm_AllocFlags)((                                                                           \
        (uint32_t)((a) < 1 ? 0 : __builtin_ctzll((unsigned long long)(a)) << 4) & AF_ALIGN_MASK    \
    )))

#define AF_DEFAULT (AF_PMM_BELOW_NONE | AF_ALIGN_AUTO)

#define MF_WRITABLE        ((StMm_MapFlags)0x00000001)
#define MF_USER            ((StMm_MapFlags)0x00000002)
#define MF_NO_CACHE        ((StMm_MapFlags)0x00000004)
#define MF_WRITETHRU_CACHE ((StMm_MapFlags)0x00000008)
#define MF_NO_EXECUTE      ((StMm_MapFlags)0x00000010)
#define MF_GLOBAL          ((StMm_MapFlags)0x00000020)
#define MF_IMMEDIATE       ((StMm_MapFlags)0x00010000)
#define MF_ZERO_FILL       ((StMm_MapFlags)0x00020000)
#define MF_NO_HUGE         ((StMm_MapFlags)0x00040000)

#define MF_POOL_LARGE_ALLOC ((StMm_MapFlags)0x00000040)
#define MF_POOL_SUBPOOL     ((StMm_MapFlags)0x00000080)

#define MF_KERNEL_DEFAULT (MF_WRITABLE)
#define MF_USER_DEFAULT   (MF_WRITABLE | MF_USER)

typedef uint32_t StMm_AllocFlags __nocast;
typedef uint32_t StMm_MapFlags __nocast;

struct StMm_CompoundFlags {
    StMm_AllocFlags alloc_flags;
    StMm_MapFlags map_flags;
};

struct StMm_ImageBacking {
    const void *base;
    size_t size;
    uintptr_t content_addr;
    size_t content_offset;
    size_t content_size;
};

typedef size_t St_PageCount __nocast;
typedef uintptr_t St_PhysFrame __nocast;
typedef uintptr_t St_VirtPage __nocast;

#endif  // __STRATA_MM_TYPES_H__
