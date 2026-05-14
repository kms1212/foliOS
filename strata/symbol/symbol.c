#include <strata/symbol.h>

#include <assert.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include <strata/compiler.h>
#include <strata/status.h>

#define SYMBOL_STATIC_MAGIC ((uint32_t)0x53594D53)

struct StSymbol_StaticEntry {
    uintptr_t address;
    uint32_t name_offset;
    uint32_t flags;
};

__weak const uint32_t StSymbolP_StaticMagic = 0;
__weak const size_t StSymbolP_StaticCount = 0;
__weak const size_t StSymbolP_StaticNamesSize = 1;
__weak const struct StSymbol_StaticEntry StSymbolP_StaticEntries[1] = {{0, 0, 0}};
__weak const char StSymbolP_StaticNames[1] = "";

static const char kernel_object_name[] = "strata";

StStatus StSymbol_Init(void)
{
    return STATUS_SUCCESS;
}

static StStatus format_address(uintptr_t address, char *buf, size_t buf_size)
{
    int len;

    if (!buf || !buf_size) return STATUS_INVALID_VALUE;

    len = snprintf(buf, buf_size, "0x%016" PRIXPTR, address);
    if (len < 0) return STATUS_UNKNOWN_ERROR;
    if ((size_t)len >= buf_size) return STATUS_BUFFER_TOO_SMALL;

    return STATUS_SUCCESS;
}

StStatus StSymbol_LookupStatic(uintptr_t address, struct StSymbol_Result *result)
{
    size_t low;
    size_t high;
    size_t index;
    const struct StSymbol_StaticEntry *entry;

    assert(result);

    result->address = address;
    result->symbol_address = 0;
    result->offset = 0;
    result->name = NULL;
    result->object_name = NULL;
    result->file = NULL;
    result->line = 0;
    result->flags = 0;

    if (StSymbolP_StaticMagic != SYMBOL_STATIC_MAGIC) return STATUS_ENTRY_NOT_FOUND;
    if (!StSymbolP_StaticCount) return STATUS_ENTRY_NOT_FOUND;

    low = 0;
    high = StSymbolP_StaticCount;
    while (low < high) {
        size_t mid = low + ((high - low) / 2);
        if (StSymbolP_StaticEntries[mid].address <= address) {
            low = mid + 1;
        } else {
            high = mid;
        }
    }

    if (!low) return STATUS_ENTRY_NOT_FOUND;

    index = low - 1;
    entry = &StSymbolP_StaticEntries[index];
    if (entry->name_offset >= StSymbolP_StaticNamesSize) return STATUS_ENTRY_NOT_FOUND;
    if (!StSymbolP_StaticNames[entry->name_offset]) return STATUS_ENTRY_NOT_FOUND;

    result->symbol_address = entry->address;
    result->offset = address - entry->address;
    result->name = &StSymbolP_StaticNames[entry->name_offset];
    result->object_name = kernel_object_name;
    result->flags = entry->flags;

    return STATUS_SUCCESS;
}

StStatus StSymbol_FormatStatic(uintptr_t address, char *buf, size_t buf_size)
{
    StStatus status;
    struct StSymbol_Result result;
    int len;

    assert(buf);

    status = StSymbol_LookupStatic(address, &result);
    if (!CHECK_SUCCESS(status)) {
        StStatus format_status = format_address(address, buf, buf_size);
        if (!CHECK_SUCCESS(format_status)) return format_status;
        return status;
    }

    if (!result.offset) {
        len = snprintf(buf, buf_size, "%s", result.name);
    } else {
        len = snprintf(buf, buf_size, "%s+0x%" PRIXPTR, result.name, result.offset);
    }

    if (len < 0) return STATUS_UNKNOWN_ERROR;
    if ((size_t)len >= buf_size) return STATUS_BUFFER_TOO_SMALL;

    return STATUS_SUCCESS;
}

StStatus StSymbol_Lookup(uintptr_t address, struct StSymbol_Result *result)
{
    assert(result);

    return StSymbol_LookupStatic(address, result);
}

StStatus StSymbol_Format(uintptr_t address, char *buf, size_t buf_size)
{
    assert(buf);

    return StSymbol_FormatStatic(address, buf, buf_size);
}
