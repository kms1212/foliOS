#ifndef __STRATA_PLAT_SYSCALL_H__
#define __STRATA_PLAT_SYSCALL_H__

#include <stdint.h>

#include <strata/plat/interrupt.h>
#include <strata/plat/syscall_num.h>

#include <strata/status.h>

StStatus StSyscallA_Init(void);
StStatus StSyscallA_Handler(
    struct StA_InterruptFrame *frame __inout, struct StIntP_Context *ctx __inout
);

#endif  // __STRATA_PLAT_SYSCALL_H__
