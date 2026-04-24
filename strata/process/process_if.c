#include "internal.h"

#include <stddef.h>
#include <stdint.h>

#include <strata/compiler.h>
#include <strata/gnt.h>
#include <strata/handle.h>
#include <strata/mm.h>
#include <strata/mm/types.h>
#include <strata/mm/utils.h>
#include <strata/process.h>
#include <strata/scheduler.h>
#include <strata/status.h>
#include <strata/thread.h>
#include <strata/utf.h>

#include "sidl/process.module.h"
#include "sidl/process.types.h"

#define MODULE_NAME "process"

struct process_dispatch_context {
    struct StProcess *process;
    struct StGnt_Node *node;
};

static StStatus read_user_u64(
    struct process_dispatch_context *ctx, const uint64_t *user_ptr, uint64_t *value_out
)
{
    if (!ctx || !ctx->process || !user_ptr || !value_out) return STATUS_INVALID_VALUE;

    return StMm_ReadLocal(
        ctx->process->address_space,
        (uintptr_t)user_ptr,
        value_out,
        sizeof(*value_out)
    );
}

static StStatus write_user_u64(
    struct process_dispatch_context *ctx, uint64_t *user_ptr, uint64_t value
)
{
    if (!ctx || !ctx->process || !user_ptr) return STATUS_INVALID_VALUE;

    return StMm_WriteLocal(ctx->process->address_space, (uintptr_t)user_ptr, &value, sizeof(value));
}

static StStatus parse_decimal_id(const St_Utf32Char *token, size_t token_len, int *value_out)
{
    int value = 0;

    if (!token || !token_len) return STATUS_INVALID_VALUE;

    for (size_t i = 0; i < token_len; i++) {
        if (token[i] < U'0' || token[i] > U'9') return STATUS_INVALID_VALUE;
        value = (value * 10) + (int)(token[i] - U'0');
    }

    if (value_out) *value_out = value;

    return STATUS_SUCCESS;
}

static int is_process_node(const struct StGnt_Node *node)
{
    return node && node->parent == g_gnt_system_processes;
}

static StStatus get_current_process(struct StProcess **process_out)
{
    StStatus status;
    struct StThread *thread;

    status = StScheduler_GetCurrentThread(&thread);
    if (!CHECK_SUCCESS(status)) return status;
    if (!thread || !thread->process) return STATUS_INVALID_THREAD;

    if (process_out) *process_out = thread->process;

    return STATUS_SUCCESS;
}

static StStatus get_process_from_process_node(
    struct StGnt_Node *process_node, struct StProcess **process_out
)
{
    StStatus status;
    int process_id;
    struct StProcess *process;

    if (!is_process_node(process_node)) return STATUS_INVALID_HANDLE;

    status = parse_decimal_id(process_node->name, process_node->name_len, &process_id);
    if (!CHECK_SUCCESS(status)) return status;

    process = StProcess_FindById(process_id);
    if (!process) return STATUS_ENTRY_NOT_FOUND;

    if (process_out) *process_out = process;

    return STATUS_SUCCESS;
}

static StMm_MapFlags map_flags_from_memmap_flags(StIfPrc_MemMapFlags flags)
{
    StMm_MapFlags map_flags = MF_USER | MF_ZERO_FILL;

    if (flags & PROCESS_MEMMAPFLAGS_WRITE) {
        map_flags |= MF_WRITABLE;
    }

    if (!(flags & PROCESS_MEMMAPFLAGS_EXECUTE)) {
        map_flags |= MF_NO_EXECUTE;
    }

    return map_flags;
}

static StMm_MapFlags map_flags_from_memremap_flags(StIfPrc_MemRemapFlags flags)
{
    StMm_MapFlags map_flags = MF_USER;

    if (flags & PROCESS_MEMREMAPFLAGS_WRITE) {
        map_flags |= MF_WRITABLE;
    }

    if (!(flags & PROCESS_MEMREMAPFLAGS_EXECUTE)) {
        map_flags |= MF_NO_EXECUTE;
    }

    return map_flags;
}

static StStatus prc_suspend(void *context, StHandle handle)
{
    (void)context;
    (void)handle;
    return STATUS_NOT_SUPPORTED;
}

static StStatus prc_resume(void *context, StHandle handle)
{
    (void)context;
    (void)handle;
    return STATUS_NOT_SUPPORTED;
}

static StStatus prc_get_state(void *context, StHandle handle, StIfPrc_State *state)
{
    struct process_dispatch_context *ctx = (struct process_dispatch_context *)context;

    (void)handle;
    if (!ctx || !ctx->process || !state) return STATUS_INVALID_VALUE;

    *state = (StIfPrc_State)ctx->process->state;
    return STATUS_SUCCESS;
}

static StStatus prc_get_id(void *context, StHandle handle, uint64_t *pid)
{
    struct process_dispatch_context *ctx = (struct process_dispatch_context *)context;

    (void)handle;
    if (!ctx || !ctx->process || !pid) return STATUS_INVALID_VALUE;

    *pid = (uint64_t)ctx->process->id;
    return STATUS_SUCCESS;
}

static StStatus prc_terminate(void *context, StHandle handle, StStatus exit_code)
{
    StStatus status;
    struct process_dispatch_context *ctx = (struct process_dispatch_context *)context;
    struct StProcess *current_process;

    (void)handle;
    (void)exit_code;
    if (!ctx || !ctx->process || !ctx->node) return STATUS_INVALID_VALUE;

    status = get_current_process(&current_process);
    if (!CHECK_SUCCESS(status)) return status;
    if (current_process != ctx->process) return STATUS_NOT_SUPPORTED;

    ctx->process->state = PROCESS_STATE_TERMINATED;

    /*
     * The process node is retained by StHandle_GetRetained() in the syscall
     * entry path. Terminate never returns, so release it here before exit.
     */
    StGnt_ReleaseNode(ctx->node);
    StThread_Exit();
    __builtin_unreachable();
}

static StStatus prc_spawn_child(
    void *context,
    StHandle handle,
    StHandle executable,
    uint32_t arg_count,
    uint8_t **arg,
    uint32_t env_count,
    uint8_t **env,
    StHandle *child
)
{
    (void)context;
    (void)handle;
    (void)executable;
    (void)arg_count;
    (void)arg;
    (void)env_count;
    (void)env;
    (void)child;
    return STATUS_NOT_SUPPORTED;
}

static StStatus prc_map_memory(
    void *context, StHandle handle, uint64_t page_count, StIfPrc_MemMapFlags flags, uint64_t *vpn
)
{
    StStatus status;
    St_VirtPage mapped_vpn;
    struct process_dispatch_context *ctx = (struct process_dispatch_context *)context;
    uint64_t mapped_vpn_u64;

    (void)handle;
    if (!ctx || !ctx->process || !vpn || !page_count) return STATUS_INVALID_VALUE;

    status = StMm_AllocateLocalSparse(
        ctx->process->address_space,
        &mapped_vpn,
        (St_PageCount)page_count,
        AF_DEFAULT,
        map_flags_from_memmap_flags(flags)
    );
    if (!CHECK_SUCCESS(status)) return status;

    mapped_vpn_u64 = (uint64_t)mapped_vpn;
    status = write_user_u64(ctx, vpn, mapped_vpn_u64);
    if (!CHECK_SUCCESS(status)) {
        StMm_FreeLocal(ctx->process->address_space, mapped_vpn, (St_PageCount)page_count);
        return status;
    }

    return STATUS_SUCCESS;
}

static StStatus prc_map_memory_to(
    void *context, StHandle handle, uint64_t page_count, StIfPrc_MemMapFlags flags, uint64_t *vpn
)
{
    StStatus status;
    struct process_dispatch_context *ctx = (struct process_dispatch_context *)context;
    uint64_t target_vpn;

    (void)handle;
    if (!ctx || !ctx->process || !vpn || !page_count) return STATUS_INVALID_VALUE;

    status = read_user_u64(ctx, vpn, &target_vpn);
    if (!CHECK_SUCCESS(status)) return status;

    status = StMm_AllocateLocalSparseTo(
        ctx->process->address_space,
        (St_VirtPage)target_vpn,
        (St_PageCount)page_count,
        AF_DEFAULT,
        map_flags_from_memmap_flags(flags)
    );
    if (!CHECK_SUCCESS(status)) return status;

    return STATUS_SUCCESS;
}

static StStatus prc_map_file_memory(
    void *context,
    StHandle handle,
    StHandle file,
    uint64_t offset,
    uint64_t page_count,
    StIfPrc_MemFileMapFlags flags,
    uint64_t *vpn
)
{
    (void)context;
    (void)handle;
    (void)file;
    (void)offset;
    (void)page_count;
    (void)flags;
    (void)vpn;
    return STATUS_NOT_SUPPORTED;
}

static StStatus prc_map_file_memory_to(
    void *context,
    StHandle handle,
    StHandle file,
    uint64_t offset,
    uint64_t page_count,
    StIfPrc_MemFileMapFlags flags,
    uint64_t *vpn
)
{
    (void)context;
    (void)handle;
    (void)file;
    (void)offset;
    (void)page_count;
    (void)flags;
    (void)vpn;
    return STATUS_NOT_SUPPORTED;
}

static StStatus prc_remap_memory(
    void *context,
    StHandle handle,
    uint64_t old_page_count,
    uint64_t new_page_count,
    StIfPrc_MemRemapFlags flags,
    uint64_t *vpn
)
{
    struct process_dispatch_context *ctx = (struct process_dispatch_context *)context;
    StStatus status;
    uint64_t target_vpn;

    (void)handle;
    if (!ctx || !ctx->process || !vpn || !old_page_count || !new_page_count) {
        return STATUS_INVALID_VALUE;
    }

    if (old_page_count != new_page_count) return STATUS_NOT_IMPLEMENTED;
    if (flags & PROCESS_MEMREMAPFLAGS_PRESERVE_PROTECTIONS) return STATUS_SUCCESS;

    status = read_user_u64(ctx, vpn, &target_vpn);
    if (!CHECK_SUCCESS(status)) return status;

    return StMm_SetLocalPageFlags(
        ctx->process->address_space,
        (St_VirtPage)target_vpn,
        (St_PageCount)old_page_count,
        map_flags_from_memremap_flags(flags)
    );
}

static StStatus prc_unmap_memory(void *context, StHandle handle, uint64_t vpn, uint64_t page_count)
{
    struct process_dispatch_context *ctx = (struct process_dispatch_context *)context;

    (void)handle;
    if (!ctx || !ctx->process || !page_count) return STATUS_INVALID_VALUE;

    StMm_FreeLocal(ctx->process->address_space, (St_VirtPage)vpn, (St_PageCount)page_count);
    return STATUS_SUCCESS;
}

static StStatus prc_schedule_sync_file_memory(
    void *context, StHandle handle, uint64_t vpn, uint64_t page_count
)
{
    (void)context;
    (void)handle;
    (void)vpn;
    (void)page_count;
    return STATUS_NOT_SUPPORTED;
}

static StStatus prc_sync_file_memory(
    void *context, StHandle handle, uint64_t vpn, uint64_t page_count
)
{
    (void)context;
    (void)handle;
    (void)vpn;
    (void)page_count;
    return STATUS_NOT_SUPPORTED;
}

static StStatus prc_invalidate_file_memory(
    void *context, StHandle handle, uint64_t vpn, uint64_t page_count
)
{
    (void)context;
    (void)handle;
    (void)vpn;
    (void)page_count;
    return STATUS_NOT_SUPPORTED;
}

static StStatus prc_lock_memory(
    void *context, StHandle handle, uint64_t vpn, uint64_t page_count, StIfPrc_MemLockFlags flags
)
{
    (void)context;
    (void)handle;
    (void)vpn;
    (void)page_count;
    (void)flags;
    return STATUS_NOT_SUPPORTED;
}

static StStatus prc_lock_all_memory(void *context, StHandle handle, StIfPrc_MemLockFlags flags)
{
    (void)context;
    (void)handle;
    (void)flags;
    return STATUS_NOT_SUPPORTED;
}

static StStatus prc_unlock_memory(void *context, StHandle handle, uint64_t vpn, uint64_t page_count)
{
    (void)context;
    (void)handle;
    (void)vpn;
    (void)page_count;
    return STATUS_NOT_SUPPORTED;
}

static StStatus prc_unlock_all_memory(void *context, StHandle handle)
{
    (void)context;
    (void)handle;
    return STATUS_NOT_SUPPORTED;
}

static StStatus prc_advise_memory(
    void *context, StHandle handle, uint64_t vpn, uint64_t page_count, StIfPrc_MemAdviseType advice
)
{
    (void)context;
    (void)handle;
    (void)vpn;
    (void)page_count;
    (void)advice;
    return STATUS_NOT_SUPPORTED;
}

static StStatus prc_check_mem_map_status(
    void *context,
    StHandle handle,
    uint64_t vpn,
    uint64_t page_count,
    StIfPrc_MemMapStatusFlags *vector
)
{
    (void)context;
    (void)handle;
    (void)vpn;
    (void)page_count;
    (void)vector;
    return STATUS_NOT_SUPPORTED;
}

static const StIfPrc_ModuleVTable g_prc_vtable = {
    .Suspend = prc_suspend,
    .Resume = prc_resume,
    .GetState = prc_get_state,
    .GetId = prc_get_id,
    .Terminate = prc_terminate,
    .SpawnChild = prc_spawn_child,
    .MapMemory = prc_map_memory,
    .MapMemoryTo = prc_map_memory_to,
    .MapFileMemory = prc_map_file_memory,
    .MapFileMemoryTo = prc_map_file_memory_to,
    .RemapMemory = prc_remap_memory,
    .UnmapMemory = prc_unmap_memory,
    .ScheduleSyncFileMemory = prc_schedule_sync_file_memory,
    .SyncFileMemory = prc_sync_file_memory,
    .InvalidateFileMemory = prc_invalidate_file_memory,
    .LockMemory = prc_lock_memory,
    .LockAllMemory = prc_lock_all_memory,
    .UnlockMemory = prc_unlock_memory,
    .UnlockAllMemory = prc_unlock_all_memory,
    .AdviseMemory = prc_advise_memory,
    .CheckMemMapStatus = prc_check_mem_map_status,
};

StStatus StProcessIf_DispatchCallArgs(
    struct StGnt_Node *node __in, StHandle_Id handle __in, uint32_t funcid __in, const long args[4]
)
{
    StStatus status;
    struct process_dispatch_context ctx;

    if (!node || !args) return STATUS_INVALID_VALUE;
    if (!is_process_node(node)) return STATUS_NOT_SUPPORTED;

    status = get_process_from_process_node(node, &ctx.process);
    if (!CHECK_SUCCESS(status)) return status;

    ctx.node = node;
    return StIfPrc_ModuleDispatchArgs(&g_prc_vtable, &ctx, handle, funcid, args);
}
