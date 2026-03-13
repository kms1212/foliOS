#ifndef __VELLUM_ASM_ISR_H__
#define __VELLUM_ASM_ISR_H__

#include <stdint.h>

#include <vellum/arch/interrupt.h>

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

typedef void (*interrupt_handler_t)(void *, struct VlA_InterruptFrame *, struct trap_regs *, int);
typedef void (*trap_handler_t)(struct VlA_InterruptFrame *, struct trap_regs *, int);

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

void VlIntP_Init(void);
status_t VlIntP_AddInterruptHandler(
    int num, void *data, interrupt_handler_t func, struct isr_handler **handler
);
status_t VlIntP_AddTrapHandler(int num, trap_handler_t func, struct isr_handler **handler);
void VlIntP_RemoveHandler(struct isr_handler *handler);

status_t VlIntP_Mask(int num);
status_t VlIntP_Unmask(int num);

uint64_t VlIntP_GetIrqCount(void);

#endif  // __VELLUM_ASM_ISR_H__
