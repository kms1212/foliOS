#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <zstd.h>

#include <strata/arch/interrupt.h>
#include <strata/arch/mmu.h>

#include <strata/plat/gdt_constants.h>
#include <strata/plat/time.h>

#include <loadst/bootinfo.h>
#include <loadst/ramdisk.h>
#include <strata/compiler.h>
#include <strata/elf.h>
#include <strata/log.h>
#include <strata/macros.h>
#include <strata/mm.h>
#include <strata/mutex.h>
#include <strata/panic.h>
#include <strata/process.h>
#include <strata/scheduler.h>
#include <strata/status.h>
#include <strata/thread.h>
#include <strata/types.h>

#define MODULE_NAME "main"

extern struct bootinfo_table_header *_pc_bootinfo_table;

struct print_state {
    uint16_t *framebuffer;
    uint32_t width, pitch, height;
    uint32_t cursor_col, cursor_row;
};

static int early_print_char(void *_state, char ch)
{
    struct print_state *state = _state;
    uint16_t *framebuffer;
    uint32_t width, height, pitch, line_diff;

    framebuffer = state->framebuffer;
    width = state->width;
    height = state->height;
    pitch = state->pitch;

    switch (ch) {
    case '\0':
        return 1;
    case '\n':
        state->cursor_row++;
    case '\r':
        state->cursor_col = 0;
        break;
    case '\t':
        state->cursor_col = (state->cursor_col + 8) & ~7;
        break;
    case '\b':
        state->cursor_col--;
        break;
    default:
        framebuffer[state->cursor_row * width + state->cursor_col] = ch | 0x0700;
        state->cursor_col++;
        break;
    }

    if (state->cursor_col >= width) {
        state->cursor_row += state->cursor_col / width;
        state->cursor_col %= width;
    }

    if (state->cursor_row >= height) {
        line_diff = state->cursor_row - height + 1;
        for (uint32_t i = 0; i < height - line_diff; i++) {
            memcpy(&framebuffer[i * width], &framebuffer[(i + line_diff) * width], pitch);
        }
        memset(&framebuffer[(height - line_diff) * width], 0, pitch * line_diff);
        state->cursor_row = height - 1;
    }

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

static int shared_value = 0;

static struct StMutex mtx;

static void thread1_main(struct StThread *th);

extern int _userexec_start;
extern int _userexec_end;

static void setup_process(void)
{
    StStatus status;
    struct StProcess *process;
    struct StElf_Object *elf;
    struct StElf64_Phdr ph;
    unsigned int ph_count;
    size_t userexec_size = (uintptr_t)&_userexec_end - (uintptr_t)&_userexec_start;
    uintptr_t entry_point;
    struct StThread *main_thread;

    status = StElf_Open(&_userexec_start, userexec_size, &elf);
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

    for (unsigned int i = 0; i < ph_count; i++) {
        status = StElf_GetProgramHeader(elf, i, &ph, sizeof(ph));
        if (!CHECK_SUCCESS(status)) {
            St_Panic(status, "failed to get program header");
        }

        if (ph.type != PT_LOAD) continue;

        status = StElf_LoadProgram(elf, i, process->address_space);
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
    const char *envs[] = {"PATH=/bin", NULL};

    status = StThread_CreateUserMain(
        process,
        entry_point,
        (St_PageCount)16,
        (St_PageCount)16,
        ARRAY_SIZE(args) - 1,
        args,
        ARRAY_SIZE(envs) - 1,
        envs,
        &main_thread
    );
    if (!CHECK_SUCCESS(status)) {
        St_Panic(status, "failed to create user thread");
    }

    process->main_thread = main_thread;

    StThread_Detach(process->main_thread);
}

static void thread3_main(struct StThread *th)
{
    uint64_t start_tick = StTimeP_GetGlobalTick();
    uint32_t time = 0;

    do {
        if (CHECK_SUCCESS(StMutex_Lock(&mtx))) {
            if (shared_value) {
                St_Panic(STATUS_SYSTEM_CORRUPTED, "asdf");
            }
            shared_value = 1;
            for (volatile int i = 0; i < 1024; i++) {
            }
            shared_value = 0;
            StMutex_Unlock(&mtx);
        }

        StThread_Sleep(1);

        time = StTimeP_GetGlobalTick() - start_tick;
    } while (time < 50);

    setup_process();
}

static void thread2_main(struct StThread *th)
{
    StStatus status;
    uint64_t start_tick = StTimeP_GetGlobalTick();
    uint32_t time = 0;
    struct StThread *new_thread;

    do {
        if (CHECK_SUCCESS(StMutex_Lock(&mtx))) {
            if (shared_value) {
                St_Panic(STATUS_SYSTEM_CORRUPTED, "asdf");
            }
            shared_value = 1;
            for (volatile int i = 0; i < 1024; i++) {
            }
            shared_value = 0;
            StMutex_Unlock(&mtx);
        }

        StThread_Sleep(1);

        time = StTimeP_GetGlobalTick() - start_tick;
    } while (time < 100);

    status = StThread_CreateKernel(thread1_main, 16, &new_thread);
    if (!CHECK_SUCCESS(status)) {
        St_Panic(status, "failed to create kernel thread");
    }

    StThread_Detach(new_thread);
}

static void thread1_main(struct StThread *th)
{
    StStatus status;
    uint64_t start_tick = StTimeP_GetGlobalTick();
    uint32_t time = 0, prev_time = 0;
    struct StThread *new_thread1, *new_thread2;
    struct StThread *waitlist[2];

    do {
        if (CHECK_SUCCESS(StMutex_Lock(&mtx))) {
            if (shared_value) {
                St_Panic(STATUS_SYSTEM_CORRUPTED, "asdf");
            }
            shared_value = 1;
            for (volatile int i = 0; i < 1024; i++) {
            }
            shared_value = 0;
            StMutex_Unlock(&mtx);
        }

        time = StTimeP_GetGlobalTick() - start_tick;
        if (time == prev_time) {
            StThread_Yield();
            continue;
        }
        prev_time = time;
    } while (time < 100);

    status = StThread_CreateKernel(thread2_main, 16, &new_thread1);
    if (!CHECK_SUCCESS(status)) {
        St_Panic(status, "failed to create kernel thread");
    }

    status = StThread_CreateKernel(thread3_main, 16, &new_thread2);
    if (!CHECK_SUCCESS(status)) {
        St_Panic(status, "failed to create kernel thread");
    }

    setup_process();

    waitlist[0] = new_thread1;
    waitlist[1] = new_thread2;
    StThread_Wait(waitlist, ARRAY_SIZE(waitlist), -1);

    StThread_Remove(new_thread1);
    StThread_Remove(new_thread2);
}

struct print_state pstate;

static void fb_print_str(int col, int row, const char *str)
{
    while (*str) {
        pstate.framebuffer[row * pstate.width + col++] = *str++ | 0x0700;
    }
}

static void thread4_main(struct StThread *th)
{
    St_PageCount total_frames, free_frames;

    char buf[512];

    for (;;) {
        StPmm_GetTotalFrameCount(&total_frames);
        StPmm_GetFreeFrameCount(&free_frames);

        snprintf(buf, sizeof(buf), "%zu / %zu", free_frames, total_frames);
        fb_print_str(80 - 23, 0, buf);

        StThread_Sleep(1);
    }
}

__section(".text") __noreturn void main(void)
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
    struct StThread *main_thread;
    struct StThread *thread1;
    struct StThread *thread4;

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
    pstate.cursor_col = pstate.cursor_row = 0;

    void *fb = pstate.framebuffer;
    memset(fb, 0, (size_t)fbent->pitch * fbent->height);

    LOG_INFO(LM_CAT_UNCLASSIFIED, "reinitializing early logger...\n");
    // StLog_EarlyInit(early_print_char, &pstate);

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

    LOG_INFO(LM_CAT_UNCLASSIFIED, "initializing thread system...\n");
    status = StThread_Init(&main_thread);
    if (!CHECK_SUCCESS(status)) {
        St_Panic(status, "failed to initialize thread system");
    }

    setup_process();

    StMutex_Init(&mtx);
    StThread_CreateKernel(thread1_main, 16, &thread1);
    StThread_Detach(thread1);

    StThread_CreateKernel(thread4_main, 16, &thread4);
    StThread_Detach(thread4);

    for (;;) {
        StScheduler_Maintain();

        if (StScheduler_CheckHasOtherRunnableThread()) {
            StThread_Yield();
        } else {
            uint32_t intstatus = StA_SaveInterrupt();
            StA_EnableInterrupt();
            StA_Halt();
            StA_RestoreInterrupt(intstatus);
        }
    }
}
