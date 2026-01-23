#include <vellum/asm/pic.h>

#include <vellum/asm/io.h>

void _pc_pic_remap_int(uint8_t master, uint8_t slave)
{
    io_out8(0x0020, 0x11);
    io_out8(0x0080, 0x00);
    io_out8(0x0021, master);
    io_out8(0x0080, 0x00);
    io_out8(0x0021, 0x04);
    io_out8(0x0080, 0x00);
    io_out8(0x0021, 0x01);
    io_out8(0x0080, 0x00);
    io_out8(0x0021, 0x00);
    
    io_out8(0x00A0, 0x11);
    io_out8(0x0080, 0x00);
    io_out8(0x00A1, slave);
    io_out8(0x0080, 0x00);
    io_out8(0x00A1, 0x02);
    io_out8(0x0080, 0x00);
    io_out8(0x00A1, 0x01);
    io_out8(0x0080, 0x00);
    io_out8(0x00A1, 0x00);
}

void _pc_pic_mask_int(int num)
{
    if (num > 0x0F) return;

    uint16_t port = num < 8 ? 0x0021 : 0x00A1;
    int irqline = num < 8 ? num : num - 8;

    io_out8(port, io_in8(port) | (1 << irqline));
}

void _pc_pic_unmask_int(int num)
{
    if (num > 0x0F) return;

    uint16_t port = num < 8 ? 0x0021 : 0x00A1;
    int irqline = num < 8 ? num : num - 8;

    io_out8(port, io_in8(port) & ~(1 << irqline));
}
