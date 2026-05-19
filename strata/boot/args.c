#include <strata/boot/args.h>

#include <assert.h>
#include <string.h>

static StStatus get_arg(
    const struct StBootArgs *args __in, uint32_t index __in, const char **arg_out __out
)
{
    assert(arg_out);

    if (!args || !args->table || !args->entry) return STATUS_INVALID_VALUE;
    if (index >= args->entry->arg_count) return STATUS_ENTRY_NOT_FOUND;

    *arg_out = &args->table->strtab[args->entry->arg_offsets[index]];
    return STATUS_SUCCESS;
}

void StBootArgs_Init(
    struct StBootArgs *args __out,
    const struct StLoad_BootInfoTableHeader *table __in,
    const struct StLoad_BootInfoEntryCommandArgs *entry __in
)
{
    assert(args);
    assert(table);
    assert(entry);

    args->table = table;
    args->entry = entry;
}

uint32_t StBootArgs_GetCount(const struct StBootArgs *args __in)
{
    if (!args || !args->entry) return 0;
    return args->entry->arg_count;
}

StStatus StBootArgs_GetArg(
    const struct StBootArgs *args __in, uint32_t index __in, const char **arg_out __out
)
{
    return get_arg(args, index, arg_out);
}

int StBootArgs_HasFlag(const struct StBootArgs *args __in, const char *name __in)
{
    uint32_t count;

    if (!args || !name) return 0;

    count = StBootArgs_GetCount(args);
    for (uint32_t i = 0; i < count; i++) {
        const char *arg;

        if (!CHECK_SUCCESS(get_arg(args, i, &arg))) return 0;
        if (strcmp(arg, name) == 0) return 1;
    }

    return 0;
}

StStatus StBootArgs_GetOptionValue(
    const struct StBootArgs *args __in, const char *name __in, const char **value_out __out
)
{
    uint32_t count;
    size_t name_len;

    assert(value_out);

    if (!args || !name) return STATUS_INVALID_VALUE;

    count = StBootArgs_GetCount(args);
    name_len = strlen(name);

    for (uint32_t i = 0; i < count; i++) {
        StStatus status;
        const char *arg;

        status = get_arg(args, i, &arg);
        if (!CHECK_SUCCESS(status)) return status;

        if (strcmp(arg, name) == 0) {
            if (i + 1 >= count) return STATUS_INVALID_VALUE;
            return get_arg(args, i + 1, value_out);
        }

        if (strncmp(arg, name, name_len) == 0 && arg[name_len] == '=') {
            *value_out = arg + name_len + 1;
            return STATUS_SUCCESS;
        }
    }

    return STATUS_ENTRY_NOT_FOUND;
}
