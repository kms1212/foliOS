#include <vellum/plat/pic.h>

#include <vellum/arch/io.h>

void _pc_pic_remap_int(uint8_t master, uint8_t slave)
{
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
    VlA_Out8(0x00A1, 0x00);
}

void _pc_pic_mask_int(int num)
{
    if (num > 0x0F) return;

    uint16_t port = num < 8 ? 0x0021 : 0x00A1;
    int irqline = num < 8 ? num : num - 8;

    VlA_Out8(port, VlA_In8(port) | (1 << irqline));
}

void _pc_pic_unmask_int(int num)
{
    if (num > 0x0F) return;

    uint16_t port = num < 8 ? 0x0021 : 0x00A1;
    int irqline = num < 8 ? num : num - 8;

    VlA_Out8(port, VlA_In8(port) & ~(1 << irqline));
}
