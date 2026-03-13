#include <vellum/plat/panic.h>

#include <stdio.h>

#include <vellum/arch/interrupt.h>
#include <vellum/arch/intrinsics/misc.h>
#include <vellum/arch/io.h>

#include <vellum/plat/bios/keyboard.h>
#include <vellum/plat/bios/video.h>
#include <vellum/plat/pic.h>
#include <vellum/plat/power.h>

static int print_char(void *, char ch)
{
    if (ch == '\n') {
        VlBiosP_WriteVideoTty('\r');
    }
    if (ch) {
        VlBiosP_WriteVideoTty(ch);
    }

    return 0;
}

__noreturn void VlP_Panic(status_t status, const char *fmt, ...)
{
    uint16_t *fbuf;
    va_list args;

    VlA_Cli();

    /* enable keyboard translation */
    VlA_Out8(0x0064, 0x60);
    VlA_Out8(0x0060, 0x63);

    _pc_pic_remap_int(0x08, 0x70);

    VlBiosP_SetVideoMode(0x03);

    fbuf = (uint16_t *)0xB8000;
    for (int i = 0; i < 80; i++) {
        fbuf[i] = 0x7000;
    }
    for (int i = 80; i < 80 * 24; i++) {
        fbuf[i] = 0x0700;
    }
    for (int i = 80 * 24; i < 80 * 25; i++) {
        fbuf[i] = 0x7000;
    }

    VlBiosP_SetVideoCursorPos(0, 0, (80 - sizeof("CRITICAL SYSTEM ERROR") + 1) / 2);
    cprintf(print_char, NULL, "CRITICAL SYSTEM ERROR");

    VlBiosP_SetVideoCursorPos(0, 2, 2);
    cprintf(print_char, NULL, "A critical error has occurred during the boot process.");
    VlBiosP_SetVideoCursorPos(0, 3, 2);
    cprintf(print_char, NULL, "Vellum encountered an unrecoverable error.");
    VlBiosP_SetVideoCursorPos(0, 4, 2);
    cprintf(print_char, NULL, "The system execution has been halted to ensure data integrity.");

    VlBiosP_SetVideoCursorPos(0, 6, 2);
    cprintf(print_char, NULL, "Status Code: %08lX", status);
    VlBiosP_SetVideoCursorPos(0, 7, 2);
    cprintf(print_char, NULL, "Description: ");

    va_start(args, fmt);
    vcprintf(print_char, NULL, fmt, args);
    va_end(args);

    VlBiosP_SetVideoCursorPos(0, 24, 2);

#ifndef NDEBUG
    cprintf(print_char, NULL, "Debugger ready");

#else
    cprintf(print_char, NULL, "Press any key to reboot");

    _pc_bios_keyboard_get_stroke(NULL, NULL);

    _pc_reboot();

#endif

    for (;;) {
        VlA_Hlt();
    }
}
