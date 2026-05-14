#ifndef __STRATA_PLAT_CPULOCAL_H__
#define __STRATA_PLAT_CPULOCAL_H__

#include <stdatomic.h>
#include <stdint.h>

#include <strata/compiler.h>
#include <strata/scheduler.h>
#include <strata/status.h>
#include <strata/thread.h>

#ifndef __STRATA_MM_ADDRESS_SPACE_REFS_DEFINED__
#    define __STRATA_MM_ADDRESS_SPACE_REFS_DEFINED__
struct StMm_AddressSpace;
typedef struct StMm_AddressSpace *StMm_AddressSpace_StrongRef __ref_strong;
typedef struct StMm_AddressSpace *StMm_AddressSpace_WeakRef __ref_weak;
typedef struct StMm_AddressSpace *StMm_AddressSpace_BorrowedRef __ref_borrowed;
typedef struct StMm_AddressSpace *StMm_AddressSpace_InternalRef __ref_internal;
#endif

struct StCpuLocalP_Data {
    uintptr_t kernel_rsp;
    uintptr_t user_rsp;
    struct StCpuLocalP_Data *self;
    atomic_uint_fast64_t irq_count;
    atomic_uint_fast64_t syscall_count;
    atomic_uint_fast64_t ctxswitch_count;
    atomic_uint_fast32_t irq_depth;
    atomic_uint_fast32_t preemption_disable_depth;
    uint32_t cpu_id;
    int is_bsp;
    struct StScheduler_Data scheduler;
    StMm_AddressSpace_InternalRef current_asp;
} __aligned(64);

StStatus StCpuLocalP_Init(void);

__always_inline struct StCpuLocalP_Data *StCpuLocalP_GetData(void)
{
    extern int _cpulocal_initialized;  // NOLINT
    struct StCpuLocalP_Data *data;
    if (!_cpulocal_initialized) return NULL;
    __asm__ volatile("mov %%gs:0x10, %0" : "=r"(data));
    return data;
}

#endif  // __STRATA_PLAT_CPULOCAL_H__
