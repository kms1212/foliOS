#include <assert.h>
#include <inttypes.h>
#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <zstd.h>

#include <uacpi/kernel_api.h>
#include <uacpi/types.h>

#include <strata/arch/interrupt.h>
#include <strata/arch/mmu_constants.h>

#include <strata/plat/cpulocal.h>
#include <strata/plat/time.h>

#include <strata/compiler.h>
#include <strata/elf.h>
#include <strata/gnt.h>
#include <strata/limits.h>
#include <strata/log.h>
#include <strata/macros.h>
#include <strata/mm.h>
#include <strata/mm/pmm.h>
#include <strata/mm/pool.h>
#include <strata/mm/types.h>
#include <strata/mm/vmm.h>
#include <strata/mutex.h>
#include <strata/panic.h>
#include <strata/process.h>
#include <strata/process_refs.h>
#include <strata/scheduler.h>
#include <strata/status.h>
#include <strata/thread.h>
#include <strata/thread_refs.h>
#include <strata/utf.h>

#include <loadst/bootinfo.h>

#define MODULE_NAME "main"

extern struct bootinfo_table_header *_pc_bootinfo_table;

struct print_state {
    uint16_t *framebuffer;
    uint32_t width, pitch, height;
    uint32_t cursor1_col, cursor1_row;
    uint32_t cursor2_col, cursor2_row;
    struct StMutex mtx1, mtx2;
};

struct print_state pstate;

int early_print_char(void *_state, char ch)
{
    StStatus status;
    struct print_state *state = _state;
    uint16_t *framebuffer;
    uint32_t width, height, line_diff;

    framebuffer = state->framebuffer;
    width = state->width;
    height = state->height;

    if (width <= 22) return 1;
    if (ch == '\0') return 1;

    status = StMutex_Lock(&state->mtx1);
    if (!CHECK_SUCCESS(status)) return 1;

    switch (ch) {
    case '\n':
        state->cursor1_row++;
    case '\r':
        state->cursor1_col = 0;
        break;
    case '\t':
        state->cursor1_col = (state->cursor1_col + 8) & ~7;
        break;
    case '\b':
        state->cursor1_col--;
        break;
    default:
        framebuffer[(state->cursor1_row * width) + state->cursor1_col] = ch | 0x0700;
        state->cursor1_col++;
        break;
    }

    if (state->cursor1_col >= width - 22) {
        state->cursor1_row += state->cursor1_col / (width - 22);
        state->cursor1_col %= width - 22;
    }

    if (state->cursor1_row >= height) {
        line_diff = state->cursor1_row - height + 1;
        for (uint32_t i = 0; i < height - line_diff; i++) {
            memcpy(
                &framebuffer[(size_t)i * width],
                &framebuffer[((size_t)i + line_diff) * width],
                sizeof(*framebuffer) * (width - 22)
            );
        }
        for (uint32_t i = 0; i < line_diff; i++) {
            memset(
                &framebuffer[((size_t)height - line_diff + i) * width],
                0,
                sizeof(*framebuffer) * (width - 22)
            );
        }
        state->cursor1_row = height - 1;
    }

    StMutex_Unlock(&state->mtx1);

    return 0;
}

int early_print_char2(void *_state, char ch)
{
    StStatus status;
    struct print_state *state = _state;
    uint16_t *framebuffer;
    uint32_t width, height, line_diff;

    framebuffer = state->framebuffer;
    width = state->width;
    height = state->height;

    if (!width) return 1;
    if (ch == '\0') return 1;

    status = StMutex_Lock(&state->mtx2);
    if (!CHECK_SUCCESS(status)) return 1;

    switch (ch) {
    case '\n':
        state->cursor2_row++;
    case '\r':
        state->cursor2_col = 58;
        break;
    case '\t':
        state->cursor2_col = (state->cursor2_col + 8) & ~7;
        break;
    case '\b':
        state->cursor2_col--;
        break;
    default:
        framebuffer[(state->cursor2_row * width) + state->cursor2_col] = ch | 0x0700;
        state->cursor2_col++;
        break;
    }

    if (state->cursor2_col >= width) {
        state->cursor2_row += state->cursor2_col / (width - 58);
        state->cursor2_col %= width - 58;
        state->cursor2_col += 58;
    }

    if (state->cursor2_row >= height) {
        line_diff = state->cursor2_row - height + 1;
        for (uint32_t i = 7; i < height - line_diff; i++) {
            memcpy(
                &framebuffer[((size_t)i * width) + width - 22],
                &framebuffer[(((size_t)i + line_diff) * width) + width - 22],
                sizeof(*framebuffer) * 22
            );
        }
        for (uint32_t i = 0; i < line_diff; i++) {
            memset(
                &framebuffer[(((size_t)height - line_diff + i) * width) + width - 22],
                0,
                sizeof(*framebuffer) * 22
            );
        }
        state->cursor2_row = height - 1;
    }

    StMutex_Unlock(&state->mtx2);

    return 0;
}

void test_zstd(void)
{
    StStatus status;

    const unsigned char compressed_data[] = {
        0x28, 0xb5, 0x2f, 0xfd, 0x04, 0x58, 0x41, 0x01, 0x00, 0x7a, 0x73, 0x74, 0x64, 0x20,
        0x6b, 0x65, 0x72, 0x6e, 0x65, 0x6c, 0x20, 0x70, 0x6f, 0x72, 0x74, 0x69, 0x6e, 0x67,
        0x20, 0x74, 0x65, 0x73, 0x74, 0x3a, 0x20, 0x68, 0x65, 0x6c, 0x6c, 0x6f, 0x20, 0x66,
        0x6f, 0x6c, 0x69, 0x4f, 0x53, 0x21, 0x0a, 0xa6, 0xa1, 0x9a, 0xfb,
    };

    size_t const max_dst_size = 40;

    void *decompressed_buffer;

    status = StPool_Allocate(max_dst_size, &decompressed_buffer);
    if (!CHECK_SUCCESS(status)) {
        LOG_DEBUG(LM_CAT_UNCLASSIFIED, "ZSTD Test: Memory allocation failed!\n");
        return;
    }

    size_t const d_size = ZSTD_decompress(
        decompressed_buffer,
        max_dst_size,
        compressed_data,
        sizeof(compressed_data)
    );

    if (ZSTD_isError(d_size)) {
        LOG_DEBUG(LM_CAT_UNCLASSIFIED, "ZSTD Decompression Error: %s\n", ZSTD_getErrorName(d_size));
    } else {
        LOG_DEBUG(
            LM_CAT_UNCLASSIFIED,
            "ZSTD Decompression Success: %s\n",
            (char *)decompressed_buffer
        );
    }

    StPool_Free(decompressed_buffer);
}

#define DUMP_GNT_ENTRY_BUFFER_SIZE 4096

struct dump_gnt_context {
    uint8_t *entry_buffer;
    St_Utf8Char *name_buf;
    St_Utf32Char *child_name;
};

static struct StGnt_Node *dump_gnt_find_registered_child(
    struct StGnt_Node *parent, const St_Utf32Char *name, size_t name_len
)
{
    struct StGnt_Node *child;

    if (!parent || parent->type == GNT_NODETYPE_LINK) return NULL;

    child = parent->children_head;
    while (child) {
        if (child->name_len == name_len &&
            memcmp(child->name, name, name_len * sizeof(St_Utf32Char)) == 0) {
            return child;
        }

        child = child->sibling;
    }

    return NULL;
}

static void dump_gnt_print_node(struct dump_gnt_context *ctx, struct StGnt_Node *node, int depth)
{
    if (node == g_gnt_root_local) {
        LOG_DEBUG(LM_CAT_UNCLASSIFIED, "%*s- /\n", depth, "");
    } else if (node == g_gnt_root_network) {
        LOG_DEBUG(LM_CAT_UNCLASSIFIED, "%*s- //\n", depth, "");
    } else if (node->type == GNT_NODETYPE_LINK) {
        (void)StUtf_Utf32ToUtf8(
            node->name, node->name_len, ctx->name_buf, NODENAME_UTF8_MAX + 1, NULL
        );
        LOG_DEBUG(
            LM_CAT_UNCLASSIFIED,
            "%*s- %s%s\n",
            depth,
            "",
            (char *)ctx->name_buf,
            (node->type == GNT_NODETYPE_DIRECTORY) ? "/" : ""
        );
        while (node->type == GNT_NODETYPE_LINK) {
            node = node->link.virtual.target_node;

            depth++;
            if (node == g_gnt_root_local) {
                LOG_DEBUG(LM_CAT_UNCLASSIFIED, "%*s-> /\n", depth, "");
            } else if (node == g_gnt_root_network) {
                LOG_DEBUG(LM_CAT_UNCLASSIFIED, "%*s-> //\n", depth, "");
            } else {
                (void)StUtf_Utf32ToUtf8(
                    node->name,
                    node->name_len,
                    ctx->name_buf,
                    NODENAME_UTF8_MAX + 1,
                    NULL
                );
                LOG_DEBUG(
                    LM_CAT_UNCLASSIFIED,
                    "%*s-> %s%c\n",
                    depth,
                    "",
                    (char *)ctx->name_buf,
                    (node->type == GNT_NODETYPE_DIRECTORY) ? '/' : ' '
                );
            }
        }
        return;
    } else {
        (void)StUtf_Utf32ToUtf8(
            node->name, node->name_len, ctx->name_buf, NODENAME_UTF8_MAX + 1, NULL
        );
        LOG_DEBUG(
            LM_CAT_UNCLASSIFIED,
            "%*s- %s%c\n",
            depth,
            "",
            (char *)ctx->name_buf,
            (node->type == GNT_NODETYPE_DIRECTORY) ? '/' : ' '
        );
    }
}

static void dump_gnt_materialize_entry(
    struct dump_gnt_context *ctx,
    struct StGnt_Node *parent,
    const struct StGnt_DirectoryEntry *entry
)
{
    struct StGnt_Node *child;

    child = dump_gnt_find_registered_child(parent, entry->name, entry->name_len);
    if (child || entry->name_len >= NODENAME_MAX) return;

    memcpy(ctx->child_name, entry->name, entry->name_len * sizeof(St_Utf32Char));
    ctx->child_name[entry->name_len] = U'\0';

    (void)StGnt_ResolvePath(parent, ctx->child_name, &child);
}

static void dump_gnt_materialize_children(
    struct dump_gnt_context *ctx, struct StGnt_Node *node, int depth
)
{
    size_t entry_count;
    uint64_t cookie = 0;
    uint64_t next_cookie = 0;
    StStatus status;

    if (node->type != GNT_NODETYPE_DIRECTORY) return;

    for (;;) {
        size_t offset = 0;

        entry_count = 0;
        next_cookie = cookie;
        status = StGnt_Iterate(
            node,
            cookie,
            ctx->entry_buffer,
            DUMP_GNT_ENTRY_BUFFER_SIZE,
            &entry_count,
            &next_cookie
        );

        if (CHECK_FAILURE(status) && status != STATUS_END_OF_LIST) {
            LOG_DEBUG(
                LM_CAT_UNCLASSIFIED,
                "%*s- <iterate failed: %08" PRIX32 ">\n",
                depth + 1,
                "",
                status
            );
            return;
        }

        for (size_t i = 0; i < entry_count; i++) {
            struct StGnt_DirectoryEntry *entry;
            size_t min_entry_len;

            if (offset + sizeof(struct StGnt_DirectoryEntry) > DUMP_GNT_ENTRY_BUFFER_SIZE) {
                LOG_DEBUG(LM_CAT_UNCLASSIFIED, "%*s- <invalid entry>\n", depth + 1, "");
                return;
            }

            entry = (struct StGnt_DirectoryEntry *)&ctx->entry_buffer[offset];
            min_entry_len = offsetof(struct StGnt_DirectoryEntry, name) +
                (entry->name_len * sizeof(St_Utf32Char));

            if (entry->entry_len < min_entry_len ||
                offset + entry->entry_len > DUMP_GNT_ENTRY_BUFFER_SIZE) {
                LOG_DEBUG(LM_CAT_UNCLASSIFIED, "%*s- <invalid entry>\n", depth + 1, "");
                return;
            }

            dump_gnt_materialize_entry(ctx, node, entry);
            offset += entry->entry_len;
        }

        if (status == STATUS_END_OF_LIST || entry_count == 0 || next_cookie == cookie) {
            return;
        }

        cookie = next_cookie;
    }
}

void dump_gnt(struct StGnt_Node *node, int depth)
{
    StStatus status;
    struct dump_gnt_context *ctx;
    struct StGnt_Node *current;
    int current_depth;

    if (!node) return;

    status = StPool_AllocateClear(sizeof(*ctx), (void **)&ctx);
    if (!CHECK_SUCCESS(status)) {
        LOG_DEBUG(
            LM_CAT_UNCLASSIFIED,
            "- <dump context allocation failed: %08" PRIX32 ">\n",
            status
        );
        return;
    }

    status = StPool_Allocate(DUMP_GNT_ENTRY_BUFFER_SIZE, (void **)&ctx->entry_buffer);
    if (!CHECK_SUCCESS(status)) {
        LOG_DEBUG(
            LM_CAT_UNCLASSIFIED,
            "- <dump buffer allocation failed: %08" PRIX32 ">\n",
            status
        );
        goto has_error;
    }

    status = StPool_Allocate(NODENAME_UTF8_MAX + 1, (void **)&ctx->name_buf);
    if (!CHECK_SUCCESS(status)) {
        LOG_DEBUG(
            LM_CAT_UNCLASSIFIED,
            "- <dump name buffer allocation failed: %08" PRIX32 ">\n",
            status
        );
        goto has_error;
    }

    status =
        StPool_Allocate((NODENAME_MAX + 1) * sizeof(*ctx->child_name), (void **)&ctx->child_name);
    if (!CHECK_SUCCESS(status)) {
        LOG_DEBUG(
            LM_CAT_UNCLASSIFIED,
            "- <dump child name buffer allocation failed: %08" PRIX32 ">\n",
            status
        );
        goto has_error;
    }

    current = node;
    current_depth = depth;

    for (;;) {
        dump_gnt_print_node(ctx, current, current_depth);
        dump_gnt_materialize_children(ctx, current, current_depth);

        if (current->type == GNT_NODETYPE_DIRECTORY && current->children_head) {
            current = current->children_head;
            current_depth++;
            continue;
        }

        while (current != node && !current->sibling) {
            current = current->parent;
            current_depth--;
            if (!current) goto has_error;
        }

        if (current == node) break;

        current = current->sibling;
    }

    if (ctx->child_name) StPool_Free(ctx->child_name);
    if (ctx->name_buf) StPool_Free(ctx->name_buf);
    if (ctx->entry_buffer) StPool_Free(ctx->entry_buffer);
    StPool_Free(ctx);
    return;

has_error:
    if (ctx->child_name) StPool_Free(ctx->child_name);
    if (ctx->name_buf) StPool_Free(ctx->name_buf);
    if (ctx->entry_buffer) StPool_Free(ctx->entry_buffer);
    StPool_Free(ctx);
}

static void thread1_main(StThread_BorrowedRef th);

static int setup_user_process(
    StProcess_StrongRef *process_out __out, StThread_StrongRef *main_thread_out __out
)
{
    assert(process_out);
    assert(main_thread_out);

    extern char _userexec_start[];  // NOLINT(readability-identifier-naming)
    extern char _userexec_end[];    // NOLINT(readability-identifier-naming)

    StStatus status;
    StProcess_StrongRef process;
    struct StElf_Object *elf;
    struct StElf64_Phdr ph;
    struct StElf_LoadOptions elf_load_options;
    unsigned int ph_count;
    size_t userexec_size = (uintptr_t)_userexec_end - (uintptr_t)_userexec_start;
    uintptr_t entry_point;
    StThread_StrongRef main_thread;
    uint32_t process_count;

    StProcess_GetCount(&process_count);

    if (process_count >= 10) return 1;

    status = StElf_Open(_userexec_start, userexec_size, &elf);
    if (!CHECK_SUCCESS(status)) {
        St_Panic(status, "failed to open elf");
    }

    status = StProcess_CreateUser(&process);
    if (!CHECK_SUCCESS(status)) {
        St_Panic(status, "failed to create user process");
    }

    status = StElf_GetProgramHeaderCount(elf, &ph_count);
    if (!CHECK_SUCCESS(status)) {
        St_Panic(status, "failed to get program header count");
    }

    elf_load_options.asp = process->address_space;
    elf_load_options.alloc_flags = AF_DEFAULT;
    elf_load_options.flags = ELF_LOAD_DEFAULT;

    for (unsigned int i = 0; i < ph_count; i++) {
        status = StElf_GetProgramHeader(elf, i, &ph, sizeof(ph));
        if (!CHECK_SUCCESS(status)) {
            St_Panic(status, "failed to get program header");
        }

        if (ph.type == PT_TLS) {
            process->tls_image_addr = (uintptr_t)ph.vaddr;
            process->tls_file_size = (size_t)ph.filesz;
            process->tls_mem_size = (size_t)ph.memsz;
            process->tls_align = (size_t)ph.addralign;
        }

        if (ph.type != PT_LOAD) continue;

        status = StElf_LoadProgram(elf, i, &elf_load_options);
        if (!CHECK_SUCCESS(status)) {
            St_Panic(status, "failed to load program");
        }
    }

    status = StElf_GetEntryPoint(elf, &entry_point);
    if (!CHECK_SUCCESS(status)) {
        St_Panic(status, "failed to get entry point");
    }

    StElf_Close(elf);

    const char *args[] = {"test", NULL};
    const char *envs[] = {"LANG=C.UTF-8", "TERM=dumb", NULL};

    status = StThread_CreateUserMain(
        process,
        entry_point,
        ARRAY_SIZE(args) - 1,
        args,
        ARRAY_SIZE(envs) - 1,
        envs,
        &main_thread
    );
    if (!CHECK_SUCCESS(status)) {
        St_Panic(status, "failed to create user thread");
    }

    cprintf(early_print_char2, &pstate, "UPROC#%d\n", main_thread->id);

    *process_out = process;
    *main_thread_out = main_thread;

    return 0;
}

static void thread3_main(StThread_BorrowedRef th)
{
    uint64_t start_tick;
    uint32_t time = 0;
    StProcess_StrongRef process = NULL;
    StThread_StrongRef main_thread = NULL;

    StTimeP_GetGlobalTick(&start_tick);

    cprintf(early_print_char2, &pstate, "KTHR3B#%d\n", th->id);

    do {
        uint64_t current_tick;

        StThread_Sleep(1);

        StTimeP_GetGlobalTick(&current_tick);
        time = current_tick - start_tick;
    } while (time < 2);

    if (setup_user_process(&process, &main_thread)) return;

    StStatus status = StThread_Detach(main_thread);
    if (!CHECK_SUCCESS(status)) {
        St_Panic(status, "failed to detach user process main thread");
    }

    cprintf(early_print_char2, &pstate, "KTHR3E#%d\n", th->id);
}

static void thread2_main(StThread_BorrowedRef th)
{
    StStatus status;
    uint64_t start_tick;
    uint32_t time = 0;

    StTimeP_GetGlobalTick(&start_tick);

    cprintf(early_print_char2, &pstate, "KTHR2B#%d\n", th->id);

    do {
        uint64_t current_tick;

        StThread_Sleep(1);

        StTimeP_GetGlobalTick(&current_tick);
        time = current_tick - start_tick;
    } while (time < 5);

    status = StThread_CreateKernel(thread1_main, TCF_DETACHED, NULL);
    if (!CHECK_SUCCESS(status)) {
        LOG_WARN(
            LM_CAT_UNCLASSIFIED,
            "thread2_main: failed to create kernel thread (status=%08X)\n",
            status
        );
        return;
    }

    cprintf(early_print_char2, &pstate, "KTHR2E#%d\n", th->id);
}

static void thread1_main(StThread_BorrowedRef th)
{
    StStatus status;
    uint64_t start_tick;
    uint32_t time = 0, prev_time = 0;
    StThread_StrongRef new_thread1, new_thread2;
    StThread_StrongRef waitlist[2];
    StProcess_StrongRef process = NULL;
    StThread_StrongRef main_thread = NULL;

    StTimeP_GetGlobalTick(&start_tick);

    cprintf(early_print_char2, &pstate, "KTHR1B#%d\n", th->id);

    do {
        uint64_t current_tick;

        StTimeP_GetGlobalTick(&current_tick);
        time = current_tick - start_tick;
        if (time == prev_time) {
            StThread_Yield();
            continue;
        }
        prev_time = time;
    } while (time < 5);

    status = StThread_CreateKernel(thread2_main, TCF_DEFAULT, &new_thread1);
    if (!CHECK_SUCCESS(status)) {
        LOG_WARN(
            LM_CAT_UNCLASSIFIED,
            "thread1_main: failed to create thread2 (status=%08X)\n",
            status
        );
        return;
    }

    status = StThread_CreateKernel(thread3_main, TCF_DEFAULT, &new_thread2);
    if (!CHECK_SUCCESS(status)) {
        LOG_WARN(
            LM_CAT_UNCLASSIFIED,
            "thread1_main: failed to create thread3 (status=%08X)\n",
            status
        );
        status = StThread_Detach(new_thread1);
        if (!CHECK_SUCCESS(status)) {
            St_Panic(status, "failed to detach thread2 after thread3 creation failure");
        }
        return;
    }

    if (!setup_user_process(&process, &main_thread)) {
        status = StThread_Detach(main_thread);
        if (!CHECK_SUCCESS(status)) {
            St_Panic(status, "failed to detach user process main thread");
        }
    }

    waitlist[0] = new_thread1;
    waitlist[1] = new_thread2;
    status = StThread_Wait(waitlist, ARRAY_SIZE(waitlist), -1);
    if (!CHECK_SUCCESS(status)) {
        St_Panic(status, "failed to wait for kernel test threads");
    }

    status = StThread_Remove(new_thread1);
    if (!CHECK_SUCCESS(status)) {
        St_Panic(status, "failed to remove thread2");
    }

    status = StThread_Remove(new_thread2);
    if (!CHECK_SUCCESS(status)) {
        St_Panic(status, "failed to remove thread3");
    }

    cprintf(early_print_char2, &pstate, "KTHR1E#%d\n", th->id);
}

static void fb_print_str(int col, int row, const char *str)
{
    while (*str) {
        pstate.framebuffer[(row * pstate.width) + col++] = *str++ | 0x0700;
    }
}

static void thread4_main(StThread_BorrowedRef th)
{
    St_PageCount total_frames, free_frames;
    uint32_t thread_count, process_count;
    uint64_t uptime_ns, syscall_count, ctxswitch_count, irq_count, idle_runtime_ns;
    uint64_t prev_sample_uptime_ns = 0;
    uint64_t prev_sample_idle_ns = 0;
    uint64_t cpu_busy_hundredths = 0;

    char buf[512];

    cprintf(early_print_char2, &pstate, "KTHR4B#%d\n", th->id);

    for (;;) {
        StPmm_GetTotalFrameCount(&total_frames);
        StPmm_GetFreeFrameCount(&free_frames);
        StThread_GetCount(&thread_count);
        StProcess_GetCount(&process_count);
        StTimeP_GetUptimeNanoseconds(&uptime_ns);
        syscall_count = atomic_load(&StCpuLocalP_GetData()->syscall_count);
        irq_count = atomic_load(&StCpuLocalP_GetData()->irq_count);
        ctxswitch_count = atomic_load(&StCpuLocalP_GetData()->ctxswitch_count);
        StScheduler_GetIdleTimeNanoseconds(&idle_runtime_ns);

        if (!prev_sample_uptime_ns) {
            prev_sample_uptime_ns = uptime_ns;
            prev_sample_idle_ns = idle_runtime_ns;
        } else if (uptime_ns - prev_sample_uptime_ns >= 250000000) {
            uint64_t window_ns = uptime_ns - prev_sample_uptime_ns;
            uint64_t idle_delta_ns = idle_runtime_ns - prev_sample_idle_ns;
            uint64_t busy_delta_ns = (idle_delta_ns >= window_ns) ? 0 : (window_ns - idle_delta_ns);

            cpu_busy_hundredths = (busy_delta_ns * 10000) / window_ns;
            prev_sample_uptime_ns = uptime_ns;
            prev_sample_idle_ns = idle_runtime_ns;
        }

        snprintf(
            buf,
            sizeof(buf),
            "CPU: %13" PRIu64 ".%02" PRIu64 "%%",
            cpu_busy_hundredths / 100,
            cpu_busy_hundredths % 100
        );
        fb_print_str(80 - 22, 0, buf);

        snprintf(buf, sizeof(buf), "MEM: %7zu / %7zu", total_frames - free_frames, total_frames);
        fb_print_str(80 - 22, 1, buf);

        snprintf(buf, sizeof(buf), "NTH: %6" PRIu32 " NPR: %5" PRIu32, thread_count, process_count);
        fb_print_str(80 - 22, 2, buf);

        snprintf(buf, sizeof(buf), "NSC: %17" PRIu64, syscall_count);
        fb_print_str(80 - 22, 3, buf);

        snprintf(buf, sizeof(buf), "NIR: %17" PRIu64, irq_count);
        fb_print_str(80 - 22, 4, buf);

        snprintf(buf, sizeof(buf), "NCS: %17" PRIu64, ctxswitch_count);
        fb_print_str(80 - 22, 5, buf);

        snprintf(
            buf,
            sizeof(buf),
            "SUT: %4" PRId64 ":%02" PRId64 ":%02" PRId64 ".%06" PRId64,
            uptime_ns / 1000 / 1000000 / 60 / 60,
            uptime_ns / 1000 / 1000000 / 60 % 60,
            uptime_ns / 1000 / 1000000 % 60,
            uptime_ns / 1000 % 1000000
        );
        fb_print_str(80 - 22, 6, buf);

        LOG_DEBUG(
            LM_CAT_UNCLASSIFIED,
            "used memory: %zu\n",
            (uint64_t)(total_frames - free_frames) * PAGE_SIZE
        );

        LOG_DEBUG(
            LM_CAT_UNCLASSIFIED,
            "cpu usage: %" PRIu64 ".%" PRIu64 "%%\n",
            cpu_busy_hundredths / 100,
            cpu_busy_hundredths % 100
        );

        StThread_Sleep(250);
    }

    cprintf(early_print_char2, &pstate, "KTHR4E#%d\n", th->id);
}

static void thread5_main(StThread_BorrowedRef th)
{
    StStatus status;
    StProcess_StrongRef process = NULL;
    StThread_StrongRef main_thread = NULL;
    StThread_StrongRef waitlist[1];

    cprintf(early_print_char2, &pstate, "KTHR5B#%d\n", th->id);

    for (;; StThread_Sleep(250)) {
        if (setup_user_process(&process, &main_thread)) continue;
        waitlist[0] = main_thread;
        status = StThread_Wait(waitlist, ARRAY_SIZE(waitlist), -1);
        if (!CHECK_SUCCESS(status)) {
            St_Panic(status, "failed to wait for user process main thread");
        }

        status = StThread_Remove(main_thread);
        if (!CHECK_SUCCESS(status)) {
            St_Panic(status, "failed to remove user process main thread");
        }
    }

    cprintf(early_print_char2, &pstate, "KTHR5E#%d\n", th->id);
}

int do_nothing(void *ctx, char ch)
{
    (void)ctx;
    (void)ch;

    return 1;
}

__noreturn void main(void)
{
    StStatus status;
    struct bootinfo_entry_header *enthdr = NULL;
    struct bootinfo_entry_command_args *caent = NULL;
    struct bootinfo_entry_loader_info *lient = NULL;
    struct bootinfo_entry_memory_map *mment = NULL;
    struct bootinfo_entry_system_disk *sdent = NULL;
    struct bootinfo_entry_acpi_rsdp *arent = NULL;
    struct bootinfo_entry_framebuffer *fbent = NULL;
    struct bootinfo_entry_default_font *dfent = NULL;
    struct bootinfo_entry_boot_graphics *bgent = NULL;
    struct bootinfo_entry_unavailable_frames *ufent = NULL;
    struct bootinfo_entry_pagetable_vpn *pvent = NULL;
    struct bootinfo_entry_ramdisk *rdent = NULL;
    St_VirtPage earlyfb_vpn;
    St_PageCount total_frames, free_frames;
    StThread_StrongRef main_thread;

    LOG_INFO(LM_CAT_UNCLASSIFIED, "starting main...\n");

    enthdr = (void *)((uintptr_t)_pc_bootinfo_table + _pc_bootinfo_table->header_size);
    for (int i = 0; i < _pc_bootinfo_table->entry_count; i++) {
        switch (enthdr->type) {
        case BET_COMMAND_ARGS:
            caent = (void *)((uintptr_t)enthdr + enthdr->header_size);
            break;
        case BET_LOADER_INFO:
            lient = (void *)((uintptr_t)enthdr + enthdr->header_size);
            break;
        case BET_MEMORY_MAP:
            mment = (void *)((uintptr_t)enthdr + enthdr->header_size);
            break;
        case BET_SYSTEM_DISK:
            sdent = (void *)((uintptr_t)enthdr + enthdr->header_size);
            break;
        case BET_ACPI_RSDP:
            arent = (void *)((uintptr_t)enthdr + enthdr->header_size);
            break;
        case BET_FRAMEBUFFER:
            fbent = (void *)((uintptr_t)enthdr + enthdr->header_size);
            break;
        case BET_DEFAULT_FONT:
            dfent = (void *)((uintptr_t)enthdr + enthdr->header_size);
            break;
        case BET_BOOT_GRAPHICS:
            bgent = (void *)((uintptr_t)enthdr + enthdr->header_size);
            break;
        case BET_UNAVAILABLE_FRAMES:
            ufent = (void *)((uintptr_t)enthdr + enthdr->header_size);
            break;
        case BET_PAGETABLE_VPN:
            pvent = (void *)((uintptr_t)enthdr + enthdr->header_size);
            break;
        case BET_RAMDISK:
            rdent = (void *)((uintptr_t)enthdr + enthdr->header_size);
            break;
        default:
            break;
        }

        enthdr = (void *)((uintptr_t)enthdr + enthdr->size);
    }

    /* hang if there's no framebuffer or not in text mode */
    if (!fbent || fbent->type != BEFT_TEXT) {
        St_Panic(STATUS_INVALID_VALUE, "no framebuffer or not in text mode");
    }

    status = StMm_MapGlobal(
        VMM_DOMAIN_IO,
        &earlyfb_vpn,
        ADDR_TO_PAGE(fbent->framebuffer_addr),
        ADDR_TO_PAGE(ALIGN(fbent->pitch * fbent->height, PAGE_SIZE)),
        NULL,
        (struct StMm_CompoundFlags){AF_DEFAULT, MF_KERNEL_DEFAULT | MF_WRITETHRU_CACHE}
    );
    if (!CHECK_SUCCESS(status)) {
        St_Panic(status, "cannot map early framebuffer");
    }

    pstate.framebuffer = (uint16_t *)PAGE_TO_VPTR(earlyfb_vpn);
    pstate.width = fbent->width;
    pstate.height = fbent->height;
    pstate.pitch = fbent->pitch;
    pstate.cursor1_col = pstate.cursor1_row = 0;
    pstate.cursor2_col = 58;
    pstate.cursor2_row = 7;

    void *fb = pstate.framebuffer;
    memset(fb, 0, (size_t)fbent->pitch * fbent->height);

    LOG_INFO(LM_CAT_UNCLASSIFIED, "reinitializing early logger...\n");
    /* Keep debugcon logger during bring-up to preserve full boot logs. */
    // StLog_EarlyInit(do_nothing, NULL);

    LOG_INFO(LM_CAT_UNCLASSIFIED, "### bootinfo table start ###\n");

    /* print entries */
    if (caent) {
        LOG_INFO(LM_CAT_UNCLASSIFIED, "command args entry:\n");
        for (uint32_t j = 0; j < caent->arg_count; j++) {
            LOG_INFO(
                LM_CAT_UNCLASSIFIED,
                "\t%s\n",
                &_pc_bootinfo_table->strtab[caent->arg_offsets[j]]
            );
        }
    }

    if (lient) {
        LOG_INFO(LM_CAT_UNCLASSIFIED, "loader info entry:\n");
        LOG_INFO(
            LM_CAT_UNCLASSIFIED,
            "\tname: %s\n",
            &_pc_bootinfo_table->strtab[lient->name_offset]
        );
        LOG_INFO(
            LM_CAT_UNCLASSIFIED,
            "\tversion: %s\n",
            &_pc_bootinfo_table->strtab[lient->version_offset]
        );
        LOG_INFO(
            LM_CAT_UNCLASSIFIED,
            "\tauthor: %s\n",
            &_pc_bootinfo_table->strtab[lient->author_offset]
        );

        if (lient->additional_entry_count > 0) {
            LOG_INFO(LM_CAT_UNCLASSIFIED, "\tadditional entries:\n");
        }
        for (uint32_t j = 0; j < lient->additional_entry_count; j++) {
            LOG_INFO(
                LM_CAT_UNCLASSIFIED,
                "\t\t%s\n",
                &_pc_bootinfo_table->strtab[lient->additional_entries[j]]
            );
        }
    }

    if (mment) {
        LOG_INFO(LM_CAT_UNCLASSIFIED, "memory map entry:\n");
        LOG_INFO(LM_CAT_UNCLASSIFIED, "\tbase             size             type\n");
        for (uint32_t j = 0; j < mment->entry_count; j++) {
            LOG_INFO(
                LM_CAT_UNCLASSIFIED,
                "\t%016" PRIX64 " %016" PRIX64 " %08" PRIX32 "\n",
                mment->entries[j].base,
                mment->entries[j].size,
                mment->entries[j].type
            );
        }
    }

    if (sdent) {
        LOG_INFO(LM_CAT_UNCLASSIFIED, "system disk entry:\n");
        LOG_INFO(LM_CAT_UNCLASSIFIED, "\tident_crc32: %08" PRIX32 "\n", sdent->ident_crc32);
        LOG_INFO(LM_CAT_UNCLASSIFIED, "\tlba              crc32\n");
        for (uint32_t j = 0; j < sdent->entry_count; j++) {
            LOG_INFO(
                LM_CAT_UNCLASSIFIED,
                "\t%016" PRIX64 " %08" PRIX32 "\n",
                sdent->entries[j].lba,
                sdent->entries[j].crc32
            );
        }
    }

    if (arent) {
        LOG_INFO(LM_CAT_UNCLASSIFIED, "acpi rsdp entry:\n");
        LOG_INFO(LM_CAT_UNCLASSIFIED, "\toemid: %.6s\n", arent->oemid);
        LOG_INFO(LM_CAT_UNCLASSIFIED, "\trevision: %02X\n", arent->revision);
        LOG_INFO(LM_CAT_UNCLASSIFIED, "\tsize: %08" PRIX32 "\n", arent->size);
        LOG_INFO(LM_CAT_UNCLASSIFIED, "\trsdt: %08" PRIX32 "\n", arent->rsdt_addr);
        LOG_INFO(LM_CAT_UNCLASSIFIED, "\txsdt: %016" PRIX64 "\n", arent->xsdt_addr);
    }

    if (dfent) {
        LOG_INFO(LM_CAT_UNCLASSIFIED, "default font entry:\n");
    }

    if (bgent) {
        LOG_INFO(LM_CAT_UNCLASSIFIED, "boot graphics entry:\n");
    }

    if (ufent) {
        LOG_INFO(LM_CAT_UNCLASSIFIED, "unavailable frames entry:\n");
        LOG_INFO(LM_CAT_UNCLASSIFIED, "\tpfn           count     type\n");
        for (uint32_t j = 0; j < ufent->entry_count; j++) {
            LOG_INFO(
                LM_CAT_UNCLASSIFIED,
                "\t%013" PRIX64 " %09" PRId32 " %01X\n",
                ufent->entries[j].pfn_base,
                ufent->entries[j].count,
                ufent->entries[j].type
            );
        }
    }

    if (pvent) {
        LOG_INFO(LM_CAT_UNCLASSIFIED, "pagetable vpn entry:\n");
        LOG_INFO(LM_CAT_UNCLASSIFIED, "\t%013" PRIX64 "\n", pvent->vpn);
    }

    if (rdent) {
        LOG_INFO(LM_CAT_UNCLASSIFIED, "boot ramdisk:\n");
        LOG_INFO(
            LM_CAT_UNCLASSIFIED,
            "\tversion=%u size=%08" PRIX32 " addr=%016" PRIX64 "\n",
            rdent->version,
            rdent->size,
            rdent->data_addr
        );
    }

    LOG_INFO(LM_CAT_UNCLASSIFIED, "### bootinfo table end ###\n");

    StPmm_GetTotalFrameCount(&total_frames);
    StPmm_GetFreeFrameCount(&free_frames);

    LOG_INFO(LM_CAT_UNCLASSIFIED, "free/total frames: %zu/%zu\n", free_frames, total_frames);

    test_zstd();

    LOG_INFO(LM_CAT_UNCLASSIFIED, "initializing the Global Node Tree...\n");
    status = StGnt_Init();
    if (!CHECK_SUCCESS(status)) {
        St_Panic(status, "failed to initialize the Global Node Tree");
    }

    LOG_INFO(LM_CAT_UNCLASSIFIED, "initializing thread system...\n");
    status = StThread_Init(&main_thread);
    if (!CHECK_SUCCESS(status)) {
        St_Panic(status, "failed to initialize thread system");
    }

    uacpi_phys_addr rsdp_base;
    extern StStatus acpi_module_main(uint64_t rsdp_base);
    uacpi_kernel_get_rsdp(&rsdp_base);
    status = acpi_module_main(rsdp_base);
    if (!CHECK_SUCCESS(status)) {
        St_Panic(status, "failed to initialize ACPI module");
    }

    struct StGnt_Node *gnt_system_hardware_node;
    status = StGnt_ResolvePath(NULL, U"/System/Hardware", &gnt_system_hardware_node);
    if (!CHECK_SUCCESS(status)) {
        St_Panic(status, "failed to resolve path /System/Hardware");
    }

    extern StStatus pci_module_main(struct StGnt_Node * parent_node);
    status = pci_module_main(gnt_system_hardware_node);
    if (!CHECK_SUCCESS(status)) {
        St_Panic(status, "failed to initialize PCI module");
    }

    dump_gnt(g_gnt_root_local, 0);
    dump_gnt(g_gnt_root_network, 0);

    status = StTimeP_StartTimer();
    if (!CHECK_SUCCESS(status)) {
        St_Panic(status, "failed to start timer");
    }

    StMutex_Init(&pstate.mtx1);
    StMutex_Init(&pstate.mtx2);
    status = StThread_CreateKernel(thread1_main, TCF_DETACHED, NULL);
    if (!CHECK_SUCCESS(status)) {
        LOG_WARN(LM_CAT_UNCLASSIFIED, "main: failed to create thread1 (status=%08X)\n", status);
    }

    status = StThread_CreateKernel(thread4_main, TCF_DETACHED, NULL);
    if (!CHECK_SUCCESS(status)) {
        LOG_WARN(LM_CAT_UNCLASSIFIED, "main: failed to create thread4 (status=%08X)\n", status);
    }

    for (;;) {
        if (StScheduler_ShouldMaintain()) {
            status = StScheduler_Maintain();
            if (!CHECK_SUCCESS(status)) {
                St_Panic(status, "failed to maintain scheduler");
            }
        }

        if (StScheduler_CheckHasOtherRunnableThread()) {
            StThread_Yield();
            continue;
        }

        uint64_t idle_start_ns;
        uint64_t idle_end_ns;
        StThread_RunDeferredReap((St_PageCount)64);

        StTimeP_GetUptimeNanoseconds(&idle_start_ns);
        uint32_t intstatus = StA_SaveInterrupt();
        StA_EnableInterruptAndHalt();
        StA_RestoreInterrupt(intstatus);
        StTimeP_GetUptimeNanoseconds(&idle_end_ns);

        if (idle_end_ns > idle_start_ns) {
            StScheduler_AccountIdleTimeNanoseconds(idle_end_ns - idle_start_ns);
        }
    }
}
