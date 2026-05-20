#include "load_folios.h"

#include <vellum/arch/interrupt.h>
#include <vellum/plat/pic.h>

__noreturn void Lf_JumpKernel(void *entry, struct StLoad_BootInfoTableHeader *btblhdr)
{
    __asm__ volatile("cli\n\t"
                     "cld\n\t"
                     "jmp *%1"
                     :
                     : "d"(btblhdr), "r"(entry)
                     : "memory", "cc");

    for (;;) {
    }
}

void Lf_PrepareKernelHandoff(void)
{
    VlA_DisableInterrupt();

    for (int irq = 0; irq < 16; irq++) {
        VlPicP_Mask(irq);
    }

    __asm__ volatile("cld" ::: "cc");
}
