#include <strata/plat/scheduler.h>

StStatus StSchedulerP_Yield(void)
{
    __asm__ volatile (
        "pushf\n\t"
        "cli\n\t"
        "int $0x20\n\t"
        "popf\n\t"
    );

    return STATUS_SUCCESS;
}
