#ifndef __STRATA_PLAT_CPULOCAL_H__
#define __STRATA_PLAT_CPULOCAL_H__

#include <stdint.h>

#include <strata/status.h>
#include <strata/compiler.h>
#include <strata/thread.h>
#include <strata/scheduler.h>

struct StCpuLocalP_Data {
    uintptr_t kernel_rsp;
    uintptr_t user_rsp;
    struct StCpuLocalP_Data *self;
    uint64_t irq_count;
    uint32_t irq_depth;
    uint32_t cpu_id;
    struct StScheduler_Data scheduler;
    int is_bsp;
} __packed __aligned(32);

StStatus StCpuLocalP_Init(void);

__always_inline struct StCpuLocalP_Data *StCpuLocalP_GetData(void)
{
    struct StCpuLocalP_Data *data;
    __asm__ volatile ("mov %%gs:0x10, %0" : "=r"(data));
    return data;
}

#endif // __STRATA_PLAT_CPULOCAL_H__
