#ifndef __STRATA_MM_TYPES_H__
#define __STRATA_MM_TYPES_H__

#include <stddef.h>
#include <stdint.h>

#include <strata/compiler.h>

#define FRAME_TO_UINT(f) ((uintptr_t)(f))
#define UINT_TO_FRAME(u) ((St_PhysFrame)(u))

#define FRAME_TO_ADDR(f) ((uintptr_t)(f) * PAGE_SIZE)
#define ADDR_TO_FRAME(a) ((St_PhysFrame)((a) / PAGE_SIZE))
#define FRAME_TO_VPTR(f) ((void *)((uintptr_t)(f) * PAGE_SIZE))
#define VPTR_TO_FRAME(v) ((St_PhysFrame)((uintptr_t)(v) / PAGE_SIZE))

#define PMM_DEFAULT ((StPmm_AllocFlags)0x00000000)

#define PMM_BELOW_MASK ((StPmm_AllocFlags)0x0000000F)
#define PMM_BELOW_NONE ((StPmm_AllocFlags)0x00000000)
#define PMM_BELOW_1M   ((StPmm_AllocFlags)0x00000001)
#define PMM_BELOW_16M  ((StPmm_AllocFlags)0x00000002)
#define PMM_BELOW_4G   ((StPmm_AllocFlags)0x00000003)

#define PMM_ALIGN_MASK ((StPmm_AllocFlags)0x000003F0)
#define PMM_ALIGN_AUTO ((StPmm_AllocFlags)0x00000000)
#define PMM_ALIGN(a)                                                                               \
    ((StPmm_AllocFlags)(((uint32_t)((a) < 1 ? 0 : __builtin_ctzll(a)) << 4) & PMM_ALIGN_MASK))

#define PAGE_TO_UINT(p) ((uintptr_t)(p))
#define UINT_TO_PAGE(u) ((St_VirtPage)(u))

#define PAGE_TO_ADDR(p) ((uintptr_t)(p) * PAGE_SIZE)
#define ADDR_TO_PAGE(a) ((St_VirtPage)((a) / PAGE_SIZE))
#define PAGE_TO_VPTR(p) ((void *)((uintptr_t)(p) * PAGE_SIZE))
#define VPTR_TO_PAGE(v) ((St_VirtPage)((uintptr_t)(v) / PAGE_SIZE))

#define VMM_DEFAULT ((StVmm_AllocFlags)0x00000000)

#define MAP_DEFAULT         ((StMm_MapFlags)0x00000000)
#define MAP_READONLY        ((StMm_MapFlags)0x00000001)
#define MAP_USER            ((StMm_MapFlags)0x00000002)
#define MAP_NO_CACHE        ((StMm_MapFlags)0x00000004)
#define MAP_WRITETHRU_CACHE ((StMm_MapFlags)0x00000008)
#define MAP_NO_EXECUTE      ((StMm_MapFlags)0x00000010)

typedef uint32_t StPmm_AllocFlags __nocast;
typedef uint32_t StVmm_AllocFlags __nocast;
typedef uint32_t StMm_MapFlags __nocast;

typedef size_t St_PageCount __nocast;
typedef uintptr_t St_PhysFrame __nocast;
typedef uintptr_t St_VirtPage __nocast;

#endif  // __STRATA_MM_TYPES_H__
