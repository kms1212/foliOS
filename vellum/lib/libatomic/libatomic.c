#include <vellum/arch/interrupt.h>

#include <stdbool.h>

uint32_t __atomic_fetch_add_4(volatile void *ptr, uint32_t val, int memorder)
{
    uint32_t prev;

    VlA_DisableInterrupt();
    prev = *(uint32_t *)ptr;
    *(uint32_t *)ptr = prev + val;
    VlA_EnableInterrupt();

    return prev;
}

uint32_t __atomic_fetch_sub_4(volatile void *ptr, uint32_t val, int memorder)
{
    uint32_t prev;

    VlA_DisableInterrupt();
    prev = *(uint32_t *)ptr;
    *(uint32_t *)ptr = prev - val;
    VlA_EnableInterrupt();

    return prev;
}

bool __atomic_compare_exchange_4(
    volatile void *ptr,
    void *expected,
    uint32_t desired,
    bool weak,
    int success_memorder,
    int failure_memorder
)
{
    uint32_t current_val = *(volatile uint32_t *)ptr;
    uint32_t expected_val = *(uint32_t *)expected;
    bool success = 0;

    VlA_DisableInterrupt();
    if (current_val == expected_val) {
        *(volatile uint32_t *)ptr = desired;
        success = 1;
    } else {
        *(uint32_t *)expected = current_val;
        success = 0;
    }
    VlA_EnableInterrupt();

    return success;
}
