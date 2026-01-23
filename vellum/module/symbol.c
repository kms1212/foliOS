#include <vellum/module.h>

#include "symbol.h"

#include <string.h>

status_t _module_find_vellum_symbol(const char *name, void **addrout)
{
    for (int i = 0; _exported_symbols[i].name; i++) {
        if (strcmp(_exported_symbols[i].name, name) == 0) {
            if (addrout) *addrout = _exported_symbols[i].ptr;
            return STATUS_SUCCESS;
        }
    }

    return STATUS_ENTRY_NOT_FOUND;
}
