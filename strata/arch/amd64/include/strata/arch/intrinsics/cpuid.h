#ifndef __STRATA_ARCH_INTRINSICS_CPUID_H__
#define __STRATA_ARCH_INTRINSICS_CPUID_H__

#include <stdint.h>

#include <strata/compiler.h>

#define CPUID_VENDOR_STRING 0x00000000
#define CPUID_FEATURES      0x00000001
#define CPUID_TLB           0x00000002
#define CPUID_SERIAL        0x00000003

#define CPUID_INTEL_EXTENDED          0x80000000
#define CPUID_INTEL_FEATURES          0x80000001
#define CPUID_INTEL_BRAND_STRING      0x80000002
#define CPUID_INTEL_BRAND_STRING_MORE 0x80000003
#define CPUID_INTEL_BRAND_STRING_END  0x80000004

__always_inline void StA_Cpuid(
    uint32_t leaf, uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx
)
{
    __asm__ volatile("cpuid" : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx) : "a"(leaf));
}

#endif  // __STRATA_ARCH_INTRINSICS_CPUID_H__
