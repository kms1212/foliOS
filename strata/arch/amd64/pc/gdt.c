#include <strata/plat/gdt.h>

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <strata/arch/gdt.h>
#include <strata/arch/intrinsics/gdt.h>
#include <strata/arch/intrinsics/ltr.h>
#include <strata/arch/tss.h>

#include <strata/plat/gdt_constants.h>
#include <strata/plat/tss.h>

static struct StA_GdtSegmentDescriptor _pc_gdt[GDT_ENTRY_COUNT];
static struct StA_Gdtr _pc_gdtr;

extern char _early_stack[];

static void gdt_load(void)
{
    __asm__ volatile(  //
        "pushq  %1\n\t"
        "lea    1f(%%rip), %%rax\n\t"
        "pushq  %%rax\n\t"
        "retfq\n\t"
        "1:\n\t"
        "mov    %0, %%ax\n\t"
        "mov    %%ax, %%ds\n\t"
        "mov    %%ax, %%es\n\t"
        "mov    %%ax, %%fs\n\t"
        "mov    %%ax, %%gs\n\t"
        "mov    %%ax, %%ss\n\t"
        :
        : "i"(SEG_SEL_KERNEL_DATA), "i"(SEG_SEL_KERNEL_CODE)
        : "memory", "rax"
    );
}

static void set_gdt_entry(int idx, uint32_t base, uint32_t limit, uint8_t access, uint32_t flags)
{
    _pc_gdt[idx].base_low = base & 0xFFFF;
    _pc_gdt[idx].base_mid = (base & 0xFF0000) >> 16;
    _pc_gdt[idx].base_high = (base & 0xFF000000) >> 24;

    _pc_gdt[idx].limit_low = limit & 0xFFFF;
    _pc_gdt[idx].limit_flags = ((flags << 4) & 0xF0) | ((limit >> 16) & 0xF);

    _pc_gdt[idx].access_byte = access;
}

static void set_gdt_system_entry(
    int idx, uint64_t base, uint32_t limit, uint8_t access, uint32_t flags
)
{
    struct StA_GdtSystemSegmentDescriptor *ssent =
        (struct StA_GdtSystemSegmentDescriptor *)&_pc_gdt[idx];

    ssent->base_low = base & 0xFFFF;
    ssent->base_mid_low = (base & 0xFF0000) >> 16;
    ssent->base_mid_high = (base & 0xFF000000) >> 24;
    ssent->base_high = (base & 0xFFFFFFFF00000000ULL) >> 32;

    ssent->limit_low = limit & 0xFFFF;
    ssent->limit_flags = ((flags << 4) & 0xF0) | ((limit >> 16) & 0xF);

    ssent->access_byte = access;
}

void StP_InitGdt(void)
{
    struct StA_Tss *tss = StP_GetTss();

    memset(&_pc_gdt, 0, sizeof(_pc_gdt));

    set_gdt_entry(SEG_SEL_KERNEL_CODE >> 3, 0x00000000, 0xFFFFF, 0x9A, 0xA);
    set_gdt_entry(SEG_SEL_KERNEL_DATA >> 3, 0x00000000, 0xFFFFF, 0x92, 0xC);
    set_gdt_entry(SEG_SEL_USER_DATA >> 3, 0x00000000, 0xFFFFF, 0xF2, 0xC);
    set_gdt_entry(SEG_SEL_USER_CODE >> 3, 0x00000000, 0xFFFFF, 0xFA, 0xA);
    set_gdt_system_entry(SEG_SEL_TSS >> 3, (uintptr_t)tss, sizeof(*tss) - 1, 0x89, 0x0);

    _pc_gdtr.size = sizeof(_pc_gdt) - 1;
    _pc_gdtr.gdt_ptr = (uint64_t)&_pc_gdt;

    StA_Lgdt(&_pc_gdtr);

    StP_InitTss();
    StP_SetTssStack((uintptr_t)_early_stack);

    gdt_load();

    StA_Ltr(SEG_SEL_TSS);
}
