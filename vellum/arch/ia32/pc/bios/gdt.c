#include <vellum/plat/gdt.h>

#include <stdio.h>

#include <vellum/arch/gdt.h>
#include <vellum/arch/intrinsics/gdt.h>

struct StA_GdtEntry _pc_gdt[8192];

void _pc_gdt_init(void)
{
    _ia32_gdtr.size = sizeof(_pc_gdt) - 1;
    _ia32_gdtr.gdt_ptr = (uint32_t)&_pc_gdt;

    _pc_gdt[_ia32_pm32_code_seg >> 3].limit_low = 0xFFFF;
    _pc_gdt[_ia32_pm32_code_seg >> 3].base_low = 0x0000;
    _pc_gdt[_ia32_pm32_code_seg >> 3].base_mid = 0x00;
    _pc_gdt[_ia32_pm32_code_seg >> 3].base_high = 0x00;
    _pc_gdt[_ia32_pm32_code_seg >> 3].access_byte.raw = 0x9A;
    _pc_gdt[_ia32_pm32_code_seg >> 3].limit_flags.raw = 0xCF;

    _pc_gdt[_ia32_pm32_data_seg >> 3].limit_low = 0xFFFF;
    _pc_gdt[_ia32_pm32_data_seg >> 3].base_low = 0x0000;
    _pc_gdt[_ia32_pm32_data_seg >> 3].base_mid = 0x00;
    _pc_gdt[_ia32_pm32_data_seg >> 3].base_high = 0x00;
    _pc_gdt[_ia32_pm32_data_seg >> 3].access_byte.raw = 0x92;
    _pc_gdt[_ia32_pm32_data_seg >> 3].limit_flags.raw = 0xCF;

    _pc_gdt[_ia32_pm16_code_seg >> 3].limit_low = 0xFFFF;
    _pc_gdt[_ia32_pm16_code_seg >> 3].base_low = 0x0000;
    _pc_gdt[_ia32_pm16_code_seg >> 3].base_mid = 0x00;
    _pc_gdt[_ia32_pm16_code_seg >> 3].base_high = 0x00;
    _pc_gdt[_ia32_pm16_code_seg >> 3].access_byte.raw = 0x9A;
    _pc_gdt[_ia32_pm16_code_seg >> 3].limit_flags.raw = 0x00;

    _pc_gdt[_ia32_pm16_data_seg >> 3].limit_low = 0xFFFF;
    _pc_gdt[_ia32_pm16_data_seg >> 3].base_low = 0x0000;
    _pc_gdt[_ia32_pm16_data_seg >> 3].base_mid = 0x00;
    _pc_gdt[_ia32_pm16_data_seg >> 3].base_high = 0x00;
    _pc_gdt[_ia32_pm16_data_seg >> 3].access_byte.raw = 0x92;
    _pc_gdt[_ia32_pm16_data_seg >> 3].limit_flags.raw = 0x00;

    VlA_Lgdt(&_ia32_gdtr);
}
