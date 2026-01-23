#include <vellum/asm/panic.h>

#include <stdio.h>

#include <vellum/asm/power.h>
#include <vellum/asm/interrupt.h>
#include <vellum/asm/io.h>
#include <vellum/asm/pic.h>
#include <vellum/asm/intrinsics/misc.h>
#include <vellum/asm/bios/video.h>
#include <vellum/asm/bios/keyboard.h>

static int print_char(void *, char ch)
{
    if (ch == '\n') {
        _pc_bios_video_write_tty('\r');
    }
    if (ch) {
        _pc_bios_video_write_tty(ch);
    }

    return 0;
}

__noreturn
void _pc_panic(status_t status, const char *fmt, ...)
{
    uint16_t *fbuf;
    va_list args;

    _i686_interrupt_disable();

    /* enable keyboard translation */
    io_out8(0x0064, 0x60);
    io_out8(0x0060, 0x63);

    _pc_pic_remap_int(0x08, 0x70);

    _pc_bios_video_set_mode(0x03);

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

    _pc_bios_video_set_cursor_pos(0, 0, (80 - sizeof("CRITICAL SYSTEM ERROR") + 1) / 2);
    cprintf(print_char, NULL, "CRITICAL SYSTEM ERROR");

    _pc_bios_video_set_cursor_pos(0, 2, 2);
    cprintf(print_char, NULL, "A critical error has occurred during the boot process.");
    _pc_bios_video_set_cursor_pos(0, 3, 2);
    cprintf(print_char, NULL, "Vellum encountered an unrecoverable error.");
    _pc_bios_video_set_cursor_pos(0, 4, 2);
    cprintf(print_char, NULL, "The system execution has been halted to ensure data integrity.");

    _pc_bios_video_set_cursor_pos(0, 6, 2);
    cprintf(print_char, NULL, "Status Code: %08X", status);
    _pc_bios_video_set_cursor_pos(0, 7, 2);
    cprintf(print_char, NULL, "Description: ");

    va_start(args, fmt);
    vcprintf(print_char, NULL, fmt, args);
    va_end(args);

    _pc_bios_video_set_cursor_pos(0, 24, 2);

#ifndef NDEBUG
    cprintf(print_char, NULL, "Debugger ready");

#else
    cprintf(print_char, NULL, "Press any key to reboot");

    _pc_bios_keyboard_get_stroke(NULL, NULL);

    _pc_reboot();

#endif
    
    for (;;) {
        _i686_halt();
    }
}
