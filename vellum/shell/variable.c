#include <vellum/shell.h>

#include <stdlib.h>
#include <string.h>

status_t VlShell_GetVariable(struct shell_instance *inst, const char *key, const char **value)
{
    for (struct shell_var *current = inst->var_list; current; current = current->next) {
        if (strncmp(current->str, key, current->key_len) == 0) {
            if (value) *value = &current->str[current->key_len + 1];

            return STATUS_SUCCESS;
        }
    }

    return STATUS_ENTRY_NOT_FOUND;
}

status_t VlShell_SetVariable(struct shell_instance *inst, const char *key, const char *value)
{
    struct shell_var *entry = NULL;
    struct shell_var *prev_entry = NULL;
    struct shell_var *tmp = NULL;
    size_t key_len = strlen(key);
    size_t value_len = strlen(value);

    for (struct shell_var *current = inst->var_list; current; current = current->next) {
        if (strncmp(current->str, key, current->key_len) == 0) {
            entry = current;
            break;
        }

        prev_entry = current;
    }

    tmp = entry;
    entry = realloc(entry, sizeof(*entry) + key_len + value_len + 2);
    if (!entry) return STATUS_UNKNOWN_ERROR;

    if (prev_entry) {
        prev_entry->next = entry;
    } else if (!inst->var_list || inst->var_list == tmp) {
        inst->var_list = entry;
    } else {
        for (prev_entry = inst->var_list; prev_entry->next; prev_entry = prev_entry->next) {
        }
        prev_entry->next = entry;
    }

    entry->key_len = key_len;
    entry->value_len = value_len;

    strncpy(entry->str, key, key_len + 1);
    strncpy(&entry->str[key_len + 1], value, value_len + 1);
    entry->str[key_len + value_len + 2] = '\0';

    return STATUS_SUCCESS;
}
