#include <vellum/plat/isr.h>

#include <stdio.h>

extern struct isr_handler *_pc_isr_table[256];

static int trap_handler_called;
static size_t instr_size;

static void temp_trap_handler(struct VlA_InterruptFrame *frame, struct trap_regs *regs, int)
{
    trap_handler_called = 1;
    frame->eip += instr_size;
}

status_t _pc_instruction_test(void (*test_func)(void), size_t _instr_size, int *is_undefined)
{
    VlA_DisableInterrupt();

    struct isr_handler *orig_isr_entry = _pc_isr_table[0x06];
    struct isr_handler temp_isr_entry = {
        .next = 0,
        .is_interrupt = 0,
        .trap_handler = temp_trap_handler,
    };

    trap_handler_called = 0;
    instr_size = _instr_size;

    _pc_isr_table[0x06] = &temp_isr_entry;

    test_func();

    if (is_undefined) *is_undefined = trap_handler_called;

    _pc_isr_table[0x06] = orig_isr_entry;

    return STATUS_SUCCESS;
}
