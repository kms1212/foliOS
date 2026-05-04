#ifndef __STRATA_PLAT_INTERRUPT_H__
#define __STRATA_PLAT_INTERRUPT_H__

#include <stdint.h>

#include <strata/arch/interrupt.h>

#include <strata/compiler.h>
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

StStatus StIntP_Init(int use_ioapic __in);
StStatus StIntP_GetFirstHandler(int num __in, struct StInt_Handler **handler __out);
StStatus StIntP_SetFirstHandler(int num __in, struct StInt_Handler *handler __in);
void StIntP_UnsetFirstHandler(int num __in);

StStatus StIntP_Mask(int num __in);
StStatus StIntP_Unmask(int num __in);

uint64_t StIntP_GetIrqCount(void);

#endif  // __STRATA_PLAT_INTERRUPT_H__
