#ifndef __STRATA_SYMBOL_H__
#define __STRATA_SYMBOL_H__

#include <stddef.h>
#include <stdint.h>

#include <strata/compiler.h>
#include <strata/status.h>

/** Result of resolving an address to symbol metadata. */
struct StSymbol_Result {
    /** Address originally requested by the caller. */
    uintptr_t address;
    /** Base address of the resolved symbol. */
    uintptr_t symbol_address;
    /** address - symbol_address. */
    uintptr_t offset;
    /** Resolved symbol name, or NULL when unavailable. */
    const char *name;
    /** Object/module name containing the symbol. */
    const char *object_name;
    /** Source file, if available. */
    const char *file;
    /** Source line, if available. */
    uint32_t line;
    /** Symbol-source specific flags. */
    uint32_t flags;
};

/** Initialize symbol lookup state. Safe to call before runtime sources exist. */
StStatus StSymbol_Init(void);

/** Resolve an address through the static built-in symbol table. */
StStatus StSymbol_LookupStatic(uintptr_t address __in, struct StSymbol_Result *result __out);
/** Format an address using only static symbol information. */
StStatus StSymbol_FormatStatic(uintptr_t address __in, char *buf __out __buf, size_t buf_size __in);

/** Resolve an address through the generic symbol provider chain. */
StStatus StSymbol_Lookup(uintptr_t address __in, struct StSymbol_Result *result __out);
/** Format an address through the generic symbol provider chain. */
StStatus StSymbol_Format(uintptr_t address __in, char *buf __out __buf, size_t buf_size __in);

#endif  // __STRATA_SYMBOL_H__
