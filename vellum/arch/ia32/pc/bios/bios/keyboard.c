#include <vellum/plat/bios/keyboard.h>

#include <vellum/arch/intrinsics/misc.h>
#include <vellum/arch/io.h>

#include <vellum/plat/bios/bioscall.h>
#include <vellum/plat/pic.h>

#define PIC_MASTER_DATA 0x0021
#define PIC_SLAVE_DATA  0x00A1

#define I8042_STATUS_PORT        0x0064
#define I8042_STATUS_OUTPUT_FULL 0x01
#define BIOS_KEYBOARD_IRQ_VECTOR 0x09

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

static void service_pending_keyboard_bytes(void)
{
    struct bioscall_regs regs = {0};

    for (int i = 0; i < 16; i++) {
        if (!(VlA_In8(I8042_STATUS_PORT) & I8042_STATUS_OUTPUT_FULL)) return;

        /*
            IRQ1 is masked while Vellum is running. If a scancode arrives between
            BIOS keyboard calls, the edge can be lost with the byte still sitting
            in the controller. Run the BIOS IRQ1 handler once so int 16h can see it.
        */
        VlBiosP_Call(BIOS_KEYBOARD_IRQ_VECTOR, &regs);
    }
}

static int call_keyboard_bios(struct bioscall_regs *regs)
{
    uint32_t interrupt_state = save_interrupt_state();
    uint8_t master_mask = VlA_In8(PIC_MASTER_DATA);
    uint8_t slave_mask = VlA_In8(PIC_SLAVE_DATA);
    int result;

    service_pending_keyboard_bytes();

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
