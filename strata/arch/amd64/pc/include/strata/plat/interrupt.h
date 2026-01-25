#ifndef __STRATA_PLAT_INTERRUPT_H__
#define __STRATA_PLAT_INTERRUPT_H__

#include <stdint.h>

#include <strata/arch/interrupt.h>

#include <strata/interrupt.h>
#include <strata/status.h>

struct StIntP_Context {
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t r11;
    uint64_t r10;
    uint64_t r9;
    uint64_t r8;
    uint64_t rdi;
    uint64_t rsi;
    uint64_t rbp;
    uint64_t rdx;
    uint64_t rcx;
    uint64_t rbx;
    uint64_t rax;
} __packed;

struct StInt_Handler;

StStatus StIntP_Init(void);
StStatus StIntP_GetFirstHandler(int num, struct StInt_Handler **handler);
StStatus StIntP_SetFirstHandler(int num, struct StInt_Handler *handler);
void StIntP_UnsetFirstHandler(int num);

StStatus StIntP_Mask(int num);
StStatus StIntP_Unmask(int num);

uint64_t StIntP_GetIrqCount(void);

#endif  // __STRATA_PLAT_INTERRUPT_H__
