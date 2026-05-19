#ifndef __STRATA_BOOT_ARGS_H__
#define __STRATA_BOOT_ARGS_H__

#include <stdint.h>

#include <strata/compiler.h>
#include <strata/status.h>

#include <stload/bootinfo.h>

struct StBootArgs {
    const struct StLoad_BootInfoTableHeader *table;
    const struct StLoad_BootInfoEntryCommandArgs *entry;
};

void StBootArgs_Init(
    struct StBootArgs *args __out,
    const struct StLoad_BootInfoTableHeader *table __in,
    const struct StLoad_BootInfoEntryCommandArgs *entry __in
);

uint32_t StBootArgs_GetCount(const struct StBootArgs *args __in);

StStatus StBootArgs_GetArg(
    const struct StBootArgs *args __in, uint32_t index __in, const char **arg_out __out
);

int StBootArgs_HasFlag(const struct StBootArgs *args __in, const char *name __in);

StStatus StBootArgs_GetOptionValue(
    const struct StBootArgs *args __in, const char *name __in, const char **value_out __out
);

#endif  // __STRATA_BOOT_ARGS_H__
