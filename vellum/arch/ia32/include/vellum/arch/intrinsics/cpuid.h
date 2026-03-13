#ifndef __VELLUM_ARCH_INTRINSICS_CPUID_H__
#define __VELLUM_ARCH_INTRINSICS_CPUID_H__

#include <stdint.h>

#include <vellum/compiler.h>

#define CPUID_VENDOR_STRING   0x00000000
#define CPUID_FEATURES        0x00000001
#define CPUID_TLB             0x00000002
#define CPUID_SERIAL          0x00000003
#define CPUID_CACHE_TOPOLOGY  0x00000004
#define CPUID_MONITOR_MWAIT   0x00000005
#define CPUID_POWER_MGMT      0x00000006
#define CPUID_EXT_FEATURES    0x00000007
#define CPUID_XSAVE           0x0000000D
#define CPUID_SGX_CAPABILITY  0x00000012
#define CPUID_PROCESSOR_TRACE 0x00000014
#define CPUID_TSC_INFO        0x00000015

#define CPUID_INTEL_EXTENDED          0x80000000
#define CPUID_INTEL_FEATURES          0x80000001
#define CPUID_INTEL_BRAND_STRING      0x80000002
#define CPUID_INTEL_BRAND_STRING_MORE 0x80000003
#define CPUID_INTEL_BRAND_STRING_END  0x80000004
#define CPUID_INTEL_PM_INFO_RAS_CAPS  0x80000007

__always_inline void VlA_Cpuid(
    uint32_t leaf, uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx
)
{
    __asm__ volatile("cpuid" : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx) : "a"(leaf));
}

#endif  // __VELLUM_ARCH_INTRINSICS_CPUID_H__
