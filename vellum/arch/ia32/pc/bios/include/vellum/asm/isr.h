#ifndef __VELLUM_ASM_ISR_H__
#define __VELLUM_ASM_ISR_H__

#include <stdint.h>

#include <vellum/asm/interrupt.h>

#include <vellum/status.h>

struct trap_regs {
    uint16_t gs;
    uint16_t padding1;
    uint16_t fs;
    uint16_t padding2;
    uint16_t es;
    uint16_t padding3;
    uint16_t ds;
    uint16_t padding4;
    uint32_t edi;
    uint32_t esi;
    uint32_t ebp;
    uint32_t esp;
    uint32_t ebx;
    uint32_t edx;
    uint32_t ecx;
    uint32_t eax;
} __packed;

typedef void (*interrupt_handler_t)(void *, struct interrupt_frame *, struct trap_regs *, int);
typedef void (*trap_handler_t)(struct interrupt_frame *, struct trap_regs *, int);

struct isr_handler {
    struct isr_handler *next;

    int irq_num;
    int is_interrupt;
    void *data;

    union {
        interrupt_handler_t interrupt_handler;
        trap_handler_t trap_handler;
    };
};

void _pc_isr_init(void);
status_t _pc_isr_add_interrupt_handler(int num, void *data, interrupt_handler_t func, struct isr_handler **handler);
status_t _pc_isr_add_trap_handler(int num, trap_handler_t func, struct isr_handler **handler);
void _pc_isr_remove_handler(struct isr_handler *handler);

status_t _pc_isr_mask_interrupt(int num);
status_t _pc_isr_unmask_interrupt(int num);

uint64_t _pc_get_irq_count(void);

#define isr_add_interrupt_handler _pc_isr_add_interrupt_handler

#endif // __VELLUM_ASM_ISR_H__
