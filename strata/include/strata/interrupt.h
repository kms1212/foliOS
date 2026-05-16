#ifndef __STRATA_INTERRUPT_H__
#define __STRATA_INTERRUPT_H__

#include <strata/arch/interrupt.h>

#include <strata/plat/interrupt.h>

#include <strata/compiler.h>
#include <strata/status.h>

struct StIntP_Context;

typedef void *(*StInt_HandlerFunction)(
    int, struct StA_InterruptFrame *, struct StIntP_Context *, void *
);

struct StInt_Handler {
    struct StInt_Handler *next;

    int irq_num;
    void *data;
    StInt_HandlerFunction handler;
};

StStatus StInt_CreateHandler(
    int num __in,
    void *data __in,
    StInt_HandlerFunction func __in,
    struct StInt_Handler **handler __out_optional
);
void StInt_RemoveHandler(struct StInt_Handler *handler __in);

#define StInt_MaskInterrupt   StIntP_MaskInterrupt
#define StInt_UnmaskInterrupt StIntP_UnmaskInterrupt
#define StInt_GetIrqCount     StIntP_GetIrqCount

#endif  // __STRATA_INTERRUPT_H__
