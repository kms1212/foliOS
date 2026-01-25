#ifndef __VELLUM_ASM_INTRINSICS_REGISTER_H__
#define __VELLUM_ASM_INTRINSICS_REGISTER_H__

#include <cpuid.h>

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

__always_inline uint32_t _ia32_read_cr0(void)
{
    uint32_t value;
    __asm__ volatile("mov    %%cr0, %%eax\n\t"
                     "mov    %%eax, %0\n\t"
                     : "=a"(value));
    return value;
}

__always_inline void _ia32_write_cr0(uint32_t value)
{
    __asm__ volatile("mov    %0, %%eax\n\t"
                     "mov    %%eax, %%cr0\n\t"
                     :
                     : "r"(value));
}

__always_inline uint32_t _ia32_read_cr2(void)
{
    uint32_t value;
    __asm__ volatile("mov    %%cr2, %%eax\n\t"
                     "mov    %%eax, %0\n\t"
                     : "=a"(value));
    return value;
}

__always_inline void _ia32_write_cr2(uint32_t value)
{
    __asm__ volatile("mov    %0, %%eax\n\t"
                     "mov    %%eax, %%cr2\n\t"
                     :
                     : "r"(value));
}

#define CR3_PWT 0x00000008
#define CR3_PCD 0x00000010

__always_inline uint32_t _ia32_read_cr3(void)
{
    uint32_t value;
    __asm__ volatile("mov    %%cr3, %%eax\n\t"
                     "mov    %%eax, %0\n\t"
                     : "=a"(value));
    return value;
}

__always_inline void _ia32_write_cr3(uint32_t value)
{
    __asm__ volatile("mov    %0, %%eax\n\t"
                     "mov    %%eax, %%cr3\n\t"
                     :
                     : "r"(value));
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

__always_inline uint32_t _ia32_read_cr4(void)
{
    uint32_t value;
    __asm__ volatile("mov    %%cr4, %%eax\n\t"
                     "mov    %%eax, %0\n\t"
                     : "=a"(value));
    return value;
}

__always_inline void _ia32_write_cr4(uint32_t value)
{
    __asm__ volatile("mov    %0, %%eax\n\t"
                     "mov    %%eax, %%cr4\n\t"
                     :
                     : "r"(value));
}

__always_inline uint32_t _ia32_read_cr8(void)
{
    uint32_t value;
    __asm__ volatile("mov    %%cr8, %%eax\n\t"
                     "mov    %%eax, %0\n\t"
                     : "=a"(value));
    return value;
}

__always_inline void _ia32_write_cr8(uint32_t value)
{
    __asm__ volatile("mov    %0, %%eax\n\t"
                     "mov    %%eax, %%cr8\n\t"
                     :
                     : "r"(value));
}

#endif  // __VELLUM_ASM_INTRINSICS_REGISTER_H__
