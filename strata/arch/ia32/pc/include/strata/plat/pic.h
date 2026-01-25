#ifndef __STRATA_PLAT_PIC_H__
#define __STRATA_PLAT_PIC_H__

#include <stdint.h>

#include <strata/arch/io.h>

#include <strata/compiler.h>

#define MPIC_CMD  0x0020
#define MPIC_DATA 0x0021
#define SPIC_CMD  0x00A0
#define SPIC_DATA 0x00A1

__always_inline void StPicP_Remap(uint8_t master, uint8_t slave)
{
    StIoA_Out8(MPIC_CMD, 0x11);
    StIoA_Wait();
    StIoA_Out8(MPIC_DATA, master);
    StIoA_Wait();
    StIoA_Out8(MPIC_DATA, 0x04);
    StIoA_Wait();
    StIoA_Out8(MPIC_DATA, 0x01);
    StIoA_Wait();
    StIoA_Out8(MPIC_DATA, 0xFF);

    StIoA_Out8(SPIC_CMD, 0x11);
    StIoA_Wait();
    StIoA_Out8(SPIC_DATA, slave);
    StIoA_Wait();
    StIoA_Out8(SPIC_DATA, 0x02);
    StIoA_Wait();
    StIoA_Out8(SPIC_DATA, 0x01);
    StIoA_Wait();
    StIoA_Out8(SPIC_DATA, 0xFF);
}

__always_inline void StPicP_Mask(int num)
{
    if (num > 0x0F) return;

    uint16_t port = num < 8 ? (MPIC_DATA) : (SPIC_DATA);
    int irqline = num < 8 ? num : num - 8;

    StIoA_Out8(port, StIoA_In8(port) | (1 << irqline));
}

__always_inline void StPicP_Unmask(int num)
{
    if (num > 0x0F) return;

    uint16_t port = num < 8 ? (MPIC_DATA) : (SPIC_DATA);
    int irqline = num < 8 ? num : num - 8;

    StIoA_Out8(port, StIoA_In8(port) & ~(1 << irqline));
}

__always_inline void StPicP_SendEoi(int num)
{
    if (num > 0x0F) return;
    if (num >= 0x08) {
        StIoA_Out8(SPIC_CMD, 0x20);
    }
    StIoA_Out8(MPIC_CMD, 0x20);
}

__always_inline void StPicP_Disable(void)
{
    StPicP_Remap(0x20, 0x28);
}

#endif  // __STRATA_PLAT_PIC_H__
