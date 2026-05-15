#ifndef __STRATA_ARCH_MMU_H__
#define __STRATA_ARCH_MMU_H__

#include <stdint.h>

#include <strata/arch/cpufeatures.h>
#include <strata/arch/intrinsics/invlpg.h>
#include <strata/arch/intrinsics/register.h>
#include <strata/arch/mmu_constants.h>

#include <strata/types.h>

#include <strata/mm/types.h>

typedef uint32_t StA_PageTableEntry;
typedef uint64_t StA_PaePageTableEntry;
typedef uint32_t StA_PageDirectoryEntry;
typedef uint64_t StA_PaePageDirectoryEntry;
typedef uint64_t StA_PageDirPtrTableEntry;
typedef uint64_t StA_PageMapLevel4Entry;
typedef uint64_t StA_PageMapLevel5Entry;

#define PTE_P       (1ULL << 0)
#define PTE_RW      (1ULL << 1)
#define PTE_US      (1ULL << 2)
#define PTE_PWT     (1ULL << 3)
#define PTE_PCD     (1ULL << 4)
#define PTE_A       (1ULL << 5)
#define PTE_D       (1ULL << 6)
#define PTE_PS      (1ULL << 7)
#define PTE_PAT     (1ULL << 7)   // For 4KB PTE
#define PDE_PAT     (1ULL << 12)  // For 2MB/1GB PDE/PDPTE
#define PTE_G       (1ULL << 8)
#define PTE_SW0     (1ULL << 9)
#define PTE_SW1     (1ULL << 10)
#define PTE_MANAGED (1ULL << 11)
#define PTE_XD      (1ULL << 63)

#define PTE_BASE_MASK  0x000FFFFFFFFFF000ULL
#define PTE_BASE_SHIFT 12

__always_inline void StA_InvalidatePage(St_VirtPage vpn __in)
{
    if (!g_p_cpu_features->has_invlpg) {
        StA_Invlpg(PAGE_TO_VPTR(vpn));
    } else {
        StA_WriteCr3(StA_ReadCr3());
    }
}

#endif  // __STRATA_ARCH_MMU_H__
