#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <strata/arch/interrupt.h>
#include <strata/arch/mmu.h>

#include <strata/plat/gdt_constants.h>
#include <strata/plat/time.h>

#include <loadst/bootinfo.h>
#include <loadst/ramdisk.h>
#include <strata/compiler.h>
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

static int shared_value = 0;

static struct StMutex mtx;

static void thread1_main(struct StThread *th);

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
}

static void thread2_main(struct StThread *th)
{
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

    StThread_CreateKernel(thread1_main, 0x10000, &new_thread);
    StThread_Detach(new_thread);
}

static void thread1_main(struct StThread *th)
{
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

    StThread_CreateKernel(thread2_main, 0x10000, &new_thread1);
    StThread_CreateKernel(thread3_main, 0x10000, &new_thread2);

    waitlist[0] = new_thread1;
    waitlist[1] = new_thread2;
    StThread_Wait(waitlist, ARRAY_SIZE(waitlist), -1);

    StThread_Remove(new_thread1);
    StThread_Remove(new_thread2);
}

static void setup_process(void)
{
    StStatus status;
    St_VirtPage test_vpn;
    uint8_t *test_addr;
    struct StProcess *process;

    StMm_AllocateSparse(
        VMM_DOMAIN_USER,
        &test_vpn,
        (St_PageCount)16,
        PMM_DEFAULT,
        VMM_DEFAULT,
        MAP_USER
    );

    test_addr = (uint8_t *)PAGE_TO_VPTR(test_vpn);
    test_addr[0] = 0x48;  // movabs $0xFFFF800000001000, %rax
    test_addr[1] = 0xB8;
    test_addr[2] = 0x00;
    test_addr[3] = 0x10;
    test_addr[4] = 0x00;
    test_addr[5] = 0x00;
    test_addr[6] = 0x00;
    test_addr[7] = 0x80;
    test_addr[8] = 0xFF;
    test_addr[9] = 0xFF;
    test_addr[10] = 0x48;  // mov (%rax), %rax
    test_addr[11] = 0x8B;
    test_addr[12] = 0x00;
    test_addr[13] = 0xFF;  // call *%rax
    test_addr[14] = 0xD0;
    test_addr[15] = 0xEB;  // jmp -15
    test_addr[16] = 0xEF;

    status = StProcess_CreateUser(&process, (uintptr_t)test_addr, (uintptr_t)test_addr + 0x10000);
    if (!CHECK_SUCCESS(status)) {
        St_Panic(status, "failed to create user process");
    }

    StThread_Detach(process->main_thread);
}

__attribute__((noreturn)) void main(void)
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
    struct print_state pstate;
    St_PageCount total_frames, free_frames;
    struct StThread *main_thread;
    struct StThread *thread1;

    LOG_INFO("starting main...\n");

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

    status = StMm_Map(
        VMM_DOMAIN_IO,
        &earlyfb_vpn,
        ADDR_TO_PAGE(fbent->framebuffer_addr),
        ADDR_TO_PAGE(ALIGN(fbent->pitch * fbent->height, PAGE_SIZE)),
        VMM_DEFAULT,
        MAP_WRITETHRU_CACHE
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

    LOG_INFO("reinitializing early logger...\n");
    StLog_EarlyInit(early_print_char, &pstate);

    LOG_INFO("### bootinfo table start ###\n");

    /* print entries */
    if (caent) {
        LOG_INFO("command args entry:\n");
        for (uint32_t j = 0; j < caent->arg_count; j++) {
            LOG_INFO("\t%s\n", &_pc_bootinfo_table->strtab[caent->arg_offsets[j]]);
        }
    }

    if (lient) {
        LOG_INFO("loader info entry:\n");
        LOG_INFO("\tname: %s\n", &_pc_bootinfo_table->strtab[lient->name_offset]);
        LOG_INFO("\tversion: %s\n", &_pc_bootinfo_table->strtab[lient->version_offset]);
        LOG_INFO("\tauthor: %s\n", &_pc_bootinfo_table->strtab[lient->author_offset]);

        if (lient->additional_entry_count > 0) {
            LOG_INFO("\tadditional entries:\n");
        }
        for (uint32_t j = 0; j < lient->additional_entry_count; j++) {
            LOG_INFO("\t\t%s\n", &_pc_bootinfo_table->strtab[lient->additional_entries[j]]);
        }
    }

    if (mment) {
        LOG_INFO("memory map entry:\n");
        LOG_INFO("\tbase             size             type\n");
        for (uint32_t j = 0; j < mment->entry_count; j++) {
            LOG_INFO(
                "\t%016" PRIX64 " %016" PRIX64 " %08" PRIX32 "\n",
                mment->entries[j].base,
                mment->entries[j].size,
                mment->entries[j].type
            );
        }
    }

    if (sdent) {
        LOG_INFO("system disk entry:\n");
        LOG_INFO("\tident_crc32: %08" PRIX32 "\n", sdent->ident_crc32);
        LOG_INFO("\tlba              crc32\n");
        for (uint32_t j = 0; j < sdent->entry_count; j++) {
            LOG_INFO(
                "\t%016" PRIX64 " %08" PRIX32 "\n",
                sdent->entries[j].lba,
                sdent->entries[j].crc32
            );
        }
    }

    if (arent) {
        LOG_INFO("acpi rsdp entry:\n");
        LOG_INFO("\toemid: %.6s\n", arent->oemid);
        LOG_INFO("\trevision: %02X\n", arent->revision);
        LOG_INFO("\tsize: %08" PRIX32 "\n", arent->size);
        LOG_INFO("\trsdt: %08" PRIX32 "\n", arent->rsdt_addr);
        LOG_INFO("\txsdt: %016" PRIX64 "\n", arent->xsdt_addr);
    }

    if (dfent) {
        LOG_INFO("default font entry:\n");
    }

    if (bgent) {
        LOG_INFO("boot graphics entry:\n");
    }

    if (ufent) {
        LOG_INFO("unavailable frames entry:\n");
        LOG_INFO("\tpfn           count     type\n");
        for (uint32_t j = 0; j < ufent->entry_count; j++) {
            LOG_INFO(
                "\t%013" PRIX64 " %09" PRId32 " %01X\n",
                ufent->entries[j].pfn_base,
                ufent->entries[j].count,
                ufent->entries[j].type
            );
        }
    }

    if (pvent) {
        LOG_INFO("pagetable vpn entry:\n");
        LOG_INFO("\t%013" PRIX64 "\n", pvent->vpn);
    }

    if (rdent) {
        LOG_INFO("boot ramdisk:\n");
        LOG_INFO(
            "\tversion=%u size=%08" PRIX32 " addr=%016" PRIX64 "\n",
            rdent->version,
            rdent->size,
            rdent->data_addr
        );
    }

    LOG_INFO("### bootinfo table end ###\n");

    StPmm_GetTotalFrameCount(&total_frames);
    StPmm_GetFreeFrameCount(&free_frames);

    LOG_INFO("free/total frames: %zu/%zu\n", free_frames, total_frames);

    LOG_INFO("initializing multitasking...\n");
    status = StThread_Init(&main_thread);
    if (!CHECK_SUCCESS(status)) {
        St_Panic(status, "failed to initialize multitasking");
    }

    StMutex_Init(&mtx);

    StThread_EnablePreemption();

    StThread_CreateKernel(thread1_main, 0x10000, &thread1);
    StThread_Detach(thread1);

    setup_process();

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
