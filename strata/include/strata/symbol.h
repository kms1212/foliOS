#ifndef __STRATA_SYMBOL_H__
#define __STRATA_SYMBOL_H__

#include <stddef.h>
#include <stdint.h>

#include <strata/compiler.h>
#include <strata/status.h>

struct StSymbol_Result {
    uintptr_t address;
    uintptr_t symbol_address;
    uintptr_t offset;
    const char *name;
    const char *object_name;
    const char *file;
    uint32_t line;
    uint32_t flags;
};

StStatus StSymbol_Init(void);

StStatus StSymbol_LookupStatic(uintptr_t address __in, struct StSymbol_Result *result __out);
StStatus StSymbol_FormatStatic(uintptr_t address __in, char *buf __out __buf, size_t buf_size __in);

StStatus StSymbol_Lookup(uintptr_t address __in, struct StSymbol_Result *result __out);
StStatus StSymbol_Format(uintptr_t address __in, char *buf __out __buf, size_t buf_size __in);

#endif  // __STRATA_SYMBOL_H__
