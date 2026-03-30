#include <strata/module.h>

#include <strata/mm/pool.h>

static struct StModule *module_list_head = NULL;
static struct StModule *module_list_tail = NULL;

StStatus StModule_Create(struct StModule **module __out)
{
    static StModule_Id new_module_id = (StModule_Id)1;

    StStatus status;
    struct StModule *new_module;

    status = StPool_AllocateClear(sizeof(*new_module), (void **)&new_module);
    if (!CHECK_SUCCESS(status)) goto has_error;

    new_module->id = new_module_id++;

    if (!module_list_head) {
        module_list_head = module_list_tail = new_module;
    } else {
        module_list_tail->next = new_module;
        module_list_tail = new_module;
    }

    if (module) *module = new_module;

    return STATUS_SUCCESS;

has_error:
    return status;
}

void StModule_Remove(struct StModule *module __in)
{
    StPool_Free(module);
}
