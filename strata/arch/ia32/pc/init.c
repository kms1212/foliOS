#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <strata/arch/cpufeatures.h>
#include <strata/arch/gdt.h>
#include <strata/arch/io.h>
#include <strata/arch/mmu.h>

#include <strata/plat/gdt.h>
#include <strata/plat/interrupt.h>
#include <strata/plat/pic.h>

#include <strata/compiler.h>
#include <strata/interrupt.h>
#include <strata/log.h>
#include <strata/macros.h>
#include <strata/mm.h>
#include <strata/panic.h>
#include <strata/scheduler.h>
#include <strata/status.h>
#include <strata/thread.h>

#include <loadst/bootinfo.h>

#define MODULE_NAME "init"

int _pc_invlpg_undefined = 1;
int _pc_rdtsc_undefined = 1;

static void invlpg_test(void)
{
    __asm__ volatile("invlpg (%0)" : : "r"(0));
}

static void rdtsc_test(void)
{
    uint32_t low, high;
    __asm__ volatile("rdtsc" : "=a"(low), "=d"(high));
}

struct bootinfo_table_header *_pc_bootinfo_table;

static int early_print_char(void *, char ch)
{
    switch (ch) {
    case '\0':
    case '\r':
        return 1;
    default:
        StIoA_Out8(0x00E9, ch);
        return 0;
    }
}

void _pc_init(struct bootinfo_table_header *btblhdr)
{
    StStatus status;
    struct bootinfo_entry_header *enthdr = NULL;
    struct bootinfo_entry_memory_map *mment = NULL;
    struct bootinfo_entry_unavailable_frames *ufent = NULL;
    struct bootinfo_entry_pagetable_vpn *pvent = NULL;
    struct bootinfo_table_header *newbtblhdr = NULL;

    StLog_EarlyInit(early_print_char, NULL);

    LOG_INFO(LM_CAT_UNCLASSIFIED, "Starting Strata...\n");

    enthdr = (void *)((uintptr_t)btblhdr + btblhdr->header_size);
    for (int i = 0; i < btblhdr->entry_count; i++) {
        switch (enthdr->type) {
        case BET_MEMORY_MAP:
            mment = (void *)((uintptr_t)enthdr + enthdr->header_size);
            break;
        case BET_UNAVAILABLE_FRAMES:
            ufent = (void *)((uintptr_t)enthdr + enthdr->header_size);
            break;
        case BET_PAGETABLE_VPN:
            pvent = (void *)((uintptr_t)enthdr + enthdr->header_size);
            break;
        default:
            break;
        }

        enthdr = (void *)((uintptr_t)enthdr + enthdr->size);
    }

    if (!mment || !ufent || !pvent) {
        St_Panic(STATUS_ENTRY_NOT_FOUND, "required entry not found");
    }

    LOG_INFO(LM_CAT_UNCLASSIFIED, "initializing GDT...\n");
    StP_InitGdt();

    LOG_INFO(LM_CAT_UNCLASSIFIED, "initializing physical memory allocator...\n");
    status = StPmm_Init(pvent->vpn, mment, ufent);
    if (!CHECK_SUCCESS(status)) {
        St_Panic(status, "failed to initialize physical memory allocator");
    }

    LOG_INFO(LM_CAT_UNCLASSIFIED, "initializing virtual memory allocator...\n");
    status = StVmm_Init(0x00000100, 0x0009FFFF, 0x000A0000, 0x000BFFFF, 0x000C1000, 0x000FEFFF);
    if (!CHECK_SUCCESS(status)) {
        St_Panic(status, "failed to initialize virtual memory allocator");
    }

    LOG_INFO(LM_CAT_UNCLASSIFIED, "initializing memory management...\n");
    status = StMm_Init();
    if (!CHECK_SUCCESS(status)) {
        St_Panic(status, "failed to initialize memory management");
    }

    LOG_INFO(LM_CAT_UNCLASSIFIED, "relocating bootinfo table...\n");
    newbtblhdr = malloc(btblhdr->size);
    if (!newbtblhdr) {
        St_Panic(status, "cannot allocate memory for bootinfo table");
    }

    memcpy(newbtblhdr, btblhdr, btblhdr->size);

    _pc_bootinfo_table = newbtblhdr;

    LOG_INFO(
        LM_CAT_UNCLASSIFIED,
        "%p %p %p %08" PRIX32 "\n",
        (void *)_pc_bootinfo_table,
        (void *)btblhdr,
        (void *)enthdr,
        btblhdr->size
    );
}

static volatile uint64_t global_tick = 0;

uint64_t StTimeP_GetGlobalTick(void)
{
    return global_tick;
}

static void *switch_thread(struct StA_InterruptFrame *frame, struct StIntP_Context *ctx)
{
    StStatus status;
    struct StThread *current_thread, *next_thread;

    status = StScheduler_GetCurrentThread(&current_thread);
    if (!CHECK_SUCCESS(status)) return NULL;

    /* ask to scheduler */
    status = StScheduler_GetNextThread(&next_thread);
    if (!CHECK_SUCCESS(status) || !next_thread) return NULL;

    /* check thread status */
    switch (next_thread->status) {
    case THREAD_STATE_PENDING:
        next_thread->status = THREAD_STATE_RUNNING;
        break;
    case THREAD_STATE_RUNNING:
        break;
    case THREAD_STATE_BLOCKING:
        break;
    case THREAD_STATE_FINISHED:
        break;
    default:
        St_Panic(STATUS_SYSTEM_CORRUPTED, "system corrupted");
    }

    /* save current stack pointer of the previous thread */
    current_thread->kmode_stack_ptr = (void *)(ctx->pushal.esp - sizeof(*ctx) - 4);

    /* switch to next thread */
    status = StScheduler_SetCurrentThread(next_thread);
    if (!CHECK_SUCCESS(status)) return NULL;

    return next_thread->kmode_stack_ptr;
}

static void *pit_isr(
    int num, struct StA_InterruptFrame *frame, struct StIntP_Context *ctx, void *data
)
{
    global_tick++;

    if (StThread_IsPreemptionEnabled()) {
        return switch_thread(frame, ctx);
    }

    return NULL;
}

static void init_pit(void)
{
    static const uint16_t pit_value = 1193182 / 100;

    StIoA_Out8(0x0043, 0x34);
    StIoA_Out8(0x0040, pit_value & 0xFF);
    StIoA_Out8(0x0040, (pit_value >> 8) & 0xFF);

    StIntP_Unmask(0x20);
}

void _pc_init_late(void)
{
    StStatus status;

    LOG_INFO(LM_CAT_UNCLASSIFIED, "initializing ISRs...\n");
    status = StIntP_Init();
    if (!CHECK_SUCCESS(status)) {
        St_Panic(status, "failed to initialize interrupt system");
    }

    StPicP_Remap(0x20, 0x28);

    StIoA_Out8(0x0070, 0x8B);
    uint8_t temp = StIoA_In8(0x0071);
    StIoA_Out8(0x0070, 0x8B);
    StIoA_Out8(0x0071, temp & ~0x70);

    LOG_INFO(LM_CAT_UNCLASSIFIED, "checking CPU features...\n");
    status = StA_CheckCpuFeatures();
    if (!CHECK_SUCCESS(status)) {
        St_Panic(status, "failed to check CPU features");
    }

    LOG_INFO(LM_CAT_UNCLASSIFIED, "activating common CPU features...\n");
    status = StA_ActivateCommonCpuFeatures();
    if (!CHECK_SUCCESS(status)) {
        St_Panic(status, "failed to activate common CPU features");
    }

    StInt_CreateHandler(0x20, NULL, pit_isr, NULL);

    LOG_INFO(LM_CAT_UNCLASSIFIED, "initializing PIT...\n");
    init_pit();
}
