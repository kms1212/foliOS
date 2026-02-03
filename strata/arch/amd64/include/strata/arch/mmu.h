#ifndef __STRATA_ARCH_MMU_H__
#define __STRATA_ARCH_MMU_H__

#include <stdint.h>

#include <strata/arch/cpufeatures.h>
#include <strata/arch/intrinsics/invlpg.h>
#include <strata/arch/intrinsics/register.h>

#include <strata/types.h>

#include <strata/mm/types.h>

#define PAGE_SIZE 4096

union StA_PageTableEntry {
    uint32_t raw;

    struct {
        uint32_t p : 1;
        uint32_t r_w : 1;
        uint32_t u_s : 1;
        uint32_t pwt : 1;
        uint32_t pcd : 1;
        uint32_t a : 1;
        uint32_t d : 1;
        uint32_t pat : 1;
        uint32_t g : 1;
        uint32_t avl : 3;
        uint32_t base : 20;
    } __packed;
} __packed;

union StA_PaePageTableEntry {
    uint64_t raw;

    struct {
        uint64_t p : 1;
        uint64_t r_w : 1;
        uint64_t u_s : 1;
        uint64_t pwt : 1;
        uint64_t pcd : 1;
        uint64_t a : 1;
        uint64_t d : 1;
        uint64_t ps : 1;
        uint64_t g : 1;
        uint64_t avl0 : 3;
        uint64_t base : 40;
        uint64_t avl1 : 7;
        uint64_t pk : 4;
        uint64_t xd : 1;
    } __packed;
} __packed;

union StA_PageDirectoryEntry {
    uint32_t raw;

    struct {
        uint32_t p : 1;
        uint32_t r_w : 1;
        uint32_t u_s : 1;
        uint32_t pwt : 1;
        uint32_t pcd : 1;
        uint32_t a : 1;
        uint32_t avl2 : 1;
        uint32_t ps : 1;
        uint32_t avl1 : 4;
        uint32_t base : 20;
    } __packed;

    struct {
        uint32_t p : 1;
        uint32_t r_w : 1;
        uint32_t u_s : 1;
        uint32_t pwt : 1;
        uint32_t pcd : 1;
        uint32_t a : 1;
        uint32_t d : 1;
        uint32_t ps : 1;
        uint32_t g : 1;
        uint32_t avl : 3;
        uint32_t pat : 1;
        uint32_t base_high : 8;
        uint32_t : 1;
        uint32_t base_low : 10;
    } __packed huge;

    union StA_PageTableEntry recursive;
} __packed;

union StA_PaePageDirectoryEntry {
    uint64_t raw;

    struct {
        uint64_t p : 1;
        uint64_t r_w : 1;
        uint64_t u_s : 1;
        uint64_t pwt : 1;
        uint64_t pcd : 1;
        uint64_t a : 1;
        uint64_t avl0 : 1;
        uint64_t ps : 1;
        uint64_t avl1 : 4;
        uint64_t base : 40;
        uint64_t avl2 : 11;
        uint64_t xd : 1;
    } __packed;

    struct {
        uint64_t p : 1;
        uint64_t r_w : 1;
        uint64_t u_s : 1;
        uint64_t pwt : 1;
        uint64_t pcd : 1;
        uint64_t a : 1;
        uint64_t d : 1;
        uint64_t ps : 1;
        uint64_t g : 1;
        uint64_t avl0 : 3;
        uint64_t pat : 1;
        uint64_t : 8;
        uint64_t base : 31;
        uint64_t avl1 : 7;
        uint64_t pk : 4;
        uint64_t xd : 1;
    } __packed huge;
} __packed;

union StA_PageDirPtrTableEntry {
    uint64_t raw;

    struct {
        uint64_t p : 1;
        uint64_t r_w : 1;
        uint64_t u_s : 1;
        uint64_t pwt : 1;
        uint64_t pcd : 1;
        uint64_t a : 1;
        uint64_t avl0 : 1;
        uint64_t ps : 1;
        uint64_t avl1 : 4;
        uint64_t base : 40;
        uint64_t avl2 : 11;
        uint64_t xd : 1;
    } __packed;

    struct {
        uint64_t p : 1;
        uint64_t r_w : 1;
        uint64_t u_s : 1;
        uint64_t pwt : 1;
        uint64_t pcd : 1;
        uint64_t a : 1;
        uint64_t d : 1;
        uint64_t ps : 1;
        uint64_t g : 1;
        uint64_t avl0 : 3;
        uint64_t pat : 1;
        uint64_t : 17;
        uint64_t base : 22;
        uint64_t avl1 : 7;
        uint64_t pk : 4;
        uint64_t xd : 1;
    } __packed huge;
} __packed;

union StA_PageMapLevel4Entry {
    uint64_t raw;

    struct {
        uint64_t p : 1;
        uint64_t r_w : 1;
        uint64_t u_s : 1;
        uint64_t pwt : 1;
        uint64_t pcd : 1;
        uint64_t a : 1;
        uint64_t avl0 : 1;
        uint64_t : 1;
        uint64_t avl1 : 4;
        uint64_t base : 40;
        uint64_t avl2 : 11;
        uint64_t xd : 1;
    } __packed;
} __packed;

union StA_PageMapLevel5Entry {
    uint64_t raw;

    struct {
        uint64_t p : 1;
        uint64_t r_w : 1;
        uint64_t u_s : 1;
        uint64_t pwt : 1;
        uint64_t pcd : 1;
        uint64_t a : 1;
        uint64_t avl0 : 1;
        uint64_t : 1;
        uint64_t avl1 : 4;
        uint64_t base : 40;
        uint64_t avl2 : 11;
        uint64_t xd : 1;
    } __packed;
} __packed;

__always_inline void StA_InvalidatePage(St_VirtPage vpn)
{
    if (!g_p_cpu_features->has_invlpg) {
        StA_Invlpg((void *)(vpn * PAGE_SIZE));
    } else {
        StA_WriteCr3(StA_ReadCr3());
    }
}

#endif  // __STRATA_ARCH_MMU_H__
