#include <vellum/plat/pic.h>

#include <stdint.h>

#include <vellum/arch/intrinsics/io.h>

void VlPicP_RemapInterrupt(uint8_t master, uint8_t slave)
{
    uint8_t master_mask = VlA_In8(0x0021);
    uint8_t slave_mask = VlA_In8(0x00A1);

    VlA_Out8(0x0020, 0x11);
    VlA_Out8(0x0080, 0x00);
    VlA_Out8(0x0021, master);
    VlA_Out8(0x0080, 0x00);
    VlA_Out8(0x0021, 0x04);
    VlA_Out8(0x0080, 0x00);
    VlA_Out8(0x0021, 0x01);
    VlA_Out8(0x0080, 0x00);
    VlA_Out8(0x0021, 0x00);

    VlA_Out8(0x00A0, 0x11);
    VlA_Out8(0x0080, 0x00);
    VlA_Out8(0x00A1, slave);
    VlA_Out8(0x0080, 0x00);
    VlA_Out8(0x00A1, 0x02);
    VlA_Out8(0x0080, 0x00);
    VlA_Out8(0x00A1, 0x01);
    VlA_Out8(0x0080, 0x00);
    VlA_Out8(0x00A1, slave_mask);

    VlA_Out8(0x0021, master_mask);
}

void VlPicP_Mask(int num)
{
    if (num > 0x0F) return;

    uint16_t port = num < 8 ? 0x0021 : 0x00A1;
    int irqline = num < 8 ? num : num - 8;

    VlA_Out8(port, VlA_In8(port) | (1 << irqline));
}

void VlPicP_Unmask(int num)
{
    if (num > 0x0F) return;

    uint16_t port = num < 8 ? 0x0021 : 0x00A1;
    int irqline = num < 8 ? num : num - 8;

    VlA_Out8(port, VlA_In8(port) & ~(1 << irqline));
}
