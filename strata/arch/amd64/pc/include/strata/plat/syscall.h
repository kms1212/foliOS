#ifndef __STRATA_PLAT_SYSCALL_H__
#define __STRATA_PLAT_SYSCALL_H__

#include <strata/plat/interrupt.h>
#include <strata/plat/syscall_num.h>

#include <strata/status.h>

StStatus StSyscallP_Init(void);
StStatus StSyscallP_Handler(struct StA_InterruptFrame *frame, struct StIntP_Context *ctx);

#endif  // __STRATA_PLAT_SYSCALL_H__
