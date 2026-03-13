#include <vellum/plat/bios/keyboard.h>

#include <vellum/plat/bios/bioscall.h>

void VlBiosP_GetKeyboardStroke(uint8_t *scancode, char *ascii)
{
    struct bioscall_regs regs = {.a.b.h = 0x10};

    VlBiosP_Call(0x16, &regs);

    if (scancode) *scancode = regs.a.b.h;
    if (ascii) *ascii = (char)regs.a.b.l;
}

int VlBiosP_CheckKeyboardStroke(uint8_t *scancode, char *ascii)
{
    struct bioscall_regs regs = {.a.b.h = 0x11};

    VlBiosP_Call(0x16, &regs);

    if (!regs.a.w) return 1;

    if (scancode) *scancode = regs.a.b.h;
    if (ascii) *ascii = (char)regs.a.b.l;

    return 0;
}

uint16_t VlBiosP_GetKeyboardState(void)
{
    struct bioscall_regs regs = {.a.b.h = 0x12};

    VlBiosP_Call(0x16, &regs);

    return regs.a.w;
}
