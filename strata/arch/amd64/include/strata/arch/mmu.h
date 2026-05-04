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

#define STA_MMU_PTE_P   (1ULL << 0)
#define STA_MMU_PTE_RW  (1ULL << 1)
#define STA_MMU_PTE_US  (1ULL << 2)
#define STA_MMU_PTE_PWT (1ULL << 3)
#define STA_MMU_PTE_PCD (1ULL << 4)
#define STA_MMU_PTE_A   (1ULL << 5)
#define STA_MMU_PTE_D   (1ULL << 6)
#define STA_MMU_PTE_PS  (1ULL << 7)
#define STA_MMU_PTE_PAT (1ULL << 7)   // For 4KB PTE
#define STA_MMU_PDE_PAT (1ULL << 12)  // For 2MB/1GB PDE/PDPTE
#define STA_MMU_PTE_G   (1ULL << 8)
#define STA_MMU_PTE_SW0 (1ULL << 9)
#define STA_MMU_PTE_SW1 (1ULL << 10)
#define STA_MMU_PTE_SW2 (1ULL << 11)
#define STA_MMU_PTE_XD  (1ULL << 63)

#define STA_MMU_PTE_BASE_MASK 0x000FFFFFFFFFF000ULL

#define STA_MMU_GET_BASE(x) ((St_PhysFrame)(((x) & STA_MMU_PTE_BASE_MASK) >> 12))
#define STA_MMU_SET_BASE(x) (((uint64_t)(x)) << 12)

__always_inline void StA_InvalidatePage(St_VirtPage vpn)
{
    if (!g_p_cpu_features->has_invlpg) {
        StA_Invlpg((void *)(vpn * PAGE_SIZE));
    } else {
        StA_WriteCr3(StA_ReadCr3());
    }
}

#endif  // __STRATA_ARCH_MMU_H__
