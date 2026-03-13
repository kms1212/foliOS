#ifndef __VELLUM_ARCH_INTRINSICS_REGISTER_H__
#define __VELLUM_ARCH_INTRINSICS_REGISTER_H__

#include <stdint.h>

#include <vellum/compiler.h>

#define CR0_PM 0x00000001
#define CR0_MP 0x00000002
#define CR0_EM 0x00000004
#define CR0_TS 0x00000008
#define CR0_ET 0x00000010
#define CR0_NE 0x00000020
#define CR0_WP 0x00010000
#define CR0_AM 0x00040000
#define CR0_NW 0x20000000
#define CR0_CD 0x40000000
#define CR0_PG 0x80000000

__always_inline uint32_t VlA_ReadCr0(void)
{
    uint32_t value;
    __asm__ volatile("mov %%cr0, %d0\n\t" : "=r"(value));
    return value;
}

__always_inline void VlA_WriteCr0(uint32_t value)
{
    __asm__ volatile("mov %d0, %%cr0\n\t" : : "r"(value) : "memory");
}

__always_inline uint32_t VlA_ReadCr2(void)
{
    uint32_t value;
    __asm__ volatile("mov %%cr2, %d0\n\t" : "=r"(value));
    return value;
}

__always_inline void VlA_WriteCr2(uint32_t value)
{
    __asm__ volatile("mov %d0, %%cr2\n\t" : : "r"(value));
}

#define CR3_PWT 0x00000008
#define CR3_PCD 0x00000010

__always_inline uint32_t VlA_ReadCr3(void)
{
    uint32_t value;
    __asm__ volatile("mov %%cr3, %d0\n\t" : "=r"(value));
    return value;
}

__always_inline void VlA_WriteCr3(uint32_t value)
{
    __asm__ volatile("mov %d0, %%cr3\n\t" : : "r"(value) : "memory");
}

#define CR4_VME        0x00000001
#define CR4_PVI        0x00000002
#define CR4_TSD        0x00000004
#define CR4_DE         0x00000008
#define CR4_PSE        0x00000010
#define CR4_PAE        0x00000020
#define CR4_MCE        0x00000040
#define CR4_PGE        0x00000080
#define CR4_PCE        0x00000100
#define CR4_OSFXSR     0x00000200
#define CR4_OSXMMEXCPT 0x00000400
#define CR4_UMIP       0x00000800
#define CR4_LA57       0x00001000
#define CR4_VMXE       0x00002000
#define CR4_SMXE       0x00004000
#define CR4_FSGSBASE   0x00010000
#define CR4_PCIDE      0x00020000
#define CR4_OSXSAVE    0x00040000
#define CR4_SMEP       0x00080000
#define CR4_SMAP       0x00100000
#define CR4_PKE        0x00200000
#define CR4_CET        0x00400000
#define CR4_PKS        0x00800000

__always_inline uint32_t VlA_ReadCr4(void)
{
    uint32_t value;
    __asm__ volatile("mov %%cr4, %d0\n\t" : "=r"(value));
    return value;
}

__always_inline void VlA_WriteCr4(uint32_t value)
{
    __asm__ volatile("mov %d0, %%cr4\n\t" : : "r"(value) : "memory");
}

__always_inline uint32_t VlA_ReadCr8(void)
{
    uint32_t value;
    __asm__ volatile("mov %%cr8, %d0\n\t" : "=r"(value));
    return value;
}

__always_inline void VlA_WriteCr8(uint32_t value)
{
    __asm__ volatile("mov %d0, %%cr8\n\t" : : "r"(value) : "memory");
}

#endif  // __VELLUM_ARCH_INTRINSICS_REGISTER_H__
