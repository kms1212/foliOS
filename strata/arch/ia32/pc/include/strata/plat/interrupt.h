#ifndef __STRATA_PLAT_INTERRUPT_H__
#define __STRATA_PLAT_INTERRUPT_H__

#include <stdint.h>

#include <strata/arch/interrupt.h>
#include <strata/arch/registers.h>

#include <strata/interrupt.h>
#include <strata/status.h>

struct StIntP_Context {
    uint16_t gs;
    uint16_t : 16;
    uint16_t fs;
    uint16_t : 16;
    uint16_t es;
    uint16_t : 16;
    uint16_t ds;
    uint16_t : 16;
    struct StA_PushalResult pushal;
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
