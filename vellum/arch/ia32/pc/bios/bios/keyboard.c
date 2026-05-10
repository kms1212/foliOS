#include <vellum/plat/bios/keyboard.h>

#include <vellum/arch/intrinsics/misc.h>
#include <vellum/arch/io.h>

#include <vellum/plat/bios/bioscall.h>
#include <vellum/plat/pic.h>

#define PIC_MASTER_DATA 0x0021
#define PIC_SLAVE_DATA  0x00A1

static uint32_t save_interrupt_state(void)
{
    uint32_t flags;

    __asm__ volatile("pushfl\n\t"
                     "popl %0"
                     : "=r"(flags));

    return (flags & 0x0200) != 0;
}

static void restore_interrupt_state(uint32_t state)
{
    if (state) {
        VlA_Sti();
    } else {
        VlA_Cli();
    }
}

static int call_keyboard_bios(struct bioscall_regs *regs)
{
    uint32_t interrupt_state = save_interrupt_state();
    uint8_t master_mask = VlA_In8(PIC_MASTER_DATA);
    uint8_t slave_mask = VlA_In8(PIC_SLAVE_DATA);
    int result;

    VlA_Cli();

    _pc_pic_remap_int(0x08, 0x70);
    VlA_Out8(PIC_MASTER_DATA, master_mask & ~(uint8_t)(1 << 1));
    VlA_Out8(PIC_SLAVE_DATA, slave_mask);

    result = VlBiosP_CallWithInterrupts(0x16, regs);

    _pc_pic_remap_int(0x20, 0x28);
    VlA_Out8(PIC_MASTER_DATA, master_mask);
    VlA_Out8(PIC_SLAVE_DATA, slave_mask);
    restore_interrupt_state(interrupt_state);

    return result;
}

void VlBiosP_GetKeyboardStroke(uint8_t *scancode, char *ascii)
{
    struct bioscall_regs regs = {.a.b.h = 0x10};

    call_keyboard_bios(&regs);

    if (scancode) *scancode = regs.a.b.h;
    if (ascii) *ascii = (char)regs.a.b.l;
}

int VlBiosP_CheckKeyboardStroke(uint8_t *scancode, char *ascii)
{
    volatile uint16_t *head = (volatile uint16_t *)(uintptr_t)0x041A;
    volatile uint16_t *tail = (volatile uint16_t *)(uintptr_t)0x041C;
    struct bioscall_regs regs = {.a.b.h = 0x11};

    call_keyboard_bios(&regs);

    if (*head == *tail) return 1;

    VlBiosP_GetKeyboardStroke(scancode, ascii);
    return 0;
}

uint16_t VlBiosP_GetKeyboardState(void)
{
    struct bioscall_regs regs = {.a.b.h = 0x12};

    call_keyboard_bios(&regs);

    return regs.a.w;
}
