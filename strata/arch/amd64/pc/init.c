#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <strata/arch/cpufeatures.h>
#include <strata/arch/gdt.h>
#include <strata/arch/io.h>
#include <strata/arch/mmu.h>

#include <strata/plat/cpulocal.h>
#include <strata/plat/gdt.h>
#include <strata/plat/interrupt.h>
#include <strata/plat/mmu.h>
#include <strata/plat/pic.h>
#include <strata/plat/syscall.h>
#include <strata/plat/time.h>

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

#define VMM_DOMAIN_USER_VPN_BASE  ((St_VirtPage)0x0000000000200ULL)
#define VMM_DOMAIN_USER_VPN_LIMIT ((St_VirtPage)0x00007FFF7FFFFULL)

#define VMM_DOMAIN_KRT_VPN_BASE  ((St_VirtPage)0xFFFF800000000ULL)
#define VMM_DOMAIN_KRT_VPN_LIMIT ((St_VirtPage)0xFFFF80007FFFFULL)

#define VMM_DOMAIN_MODULE_VPN_BASE  ((St_VirtPage)0xFFFF800080000ULL)
#define VMM_DOMAIN_MODULE_VPN_LIMIT ((St_VirtPage)0xFFFFBFFF7FFFFULL)

#define VMM_DOMAIN_IO_VPN_BASE  ((St_VirtPage)0xFFFFF00000000ULL)
#define VMM_DOMAIN_IO_VPN_LIMIT ((St_VirtPage)0xFFFFF7FFFFFFFULL)

#define VMM_DOMAIN_KERNEL_SLOW_VPN_BASE  ((St_VirtPage)0xFFFFF80000000ULL)
#define VMM_DOMAIN_KERNEL_SLOW_VPN_LIMIT ((St_VirtPage)0xFFFFFFFEFFFFFULL)

#define VMM_DOMAIN_KERNEL_FAST_VPN_BASE  ((St_VirtPage)0xFFFFFFFF80000ULL)
#define VMM_DOMAIN_KERNEL_FAST_VPN_LIMIT ((St_VirtPage)0xFFFFFFFFFFFFFULL)

__externally_visible struct bootinfo_table_header *_pc_bootinfo_table;

extern int _trampoline_load_;
extern int _trampoline_size_;

extern int _krt_start;
extern int _krt_end;

extern int _end_;

static int early_print_char(void *data, char ch)
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

static volatile uint64_t global_tick = 0;

uint64_t StTimeP_GetGlobalTick(void)
{
    return global_tick;
}

uint32_t StTimeP_GetGlobalTickFrequency(void)
{
    return 100;
}

static void *trap_isr(
    int num, struct StA_InterruptFrame *frame, struct StIntP_Context *ctx, void *data
)
{
    StSyscallP_Handler(frame, ctx);

    return NULL;
}

static void *pit_isr(
    int num, struct StA_InterruptFrame *frame, struct StIntP_Context *ctx, void *data
)
{
    StStatus status;
    struct StThread *next_thread;
    void *next_stack_ptr;

    global_tick++;

    if (StThread_IsPreemptionEnabled()) {
        status = StScheduler_GetNextThread(&next_thread);
        if (!CHECK_SUCCESS(status) || !next_thread) return NULL;

        status = StThreadP_Switch(next_thread, ctx, &next_stack_ptr);
        if (!CHECK_SUCCESS(status)) return NULL;

        return next_stack_ptr;
    }

    return NULL;
}

static void init_pit(void)
{
    static const uint16_t pit_value = 1193182 / 100;

    // rate generator (10ms, 100Hz) to channel 0
    StIoA_Out8(0x0043, 0x34);
    StIoA_Out8(0x0040, pit_value & 0xFF);
    StIoA_Out8(0x0040, (pit_value >> 8) & 0xFF);

    LOG_INFO("Clock source initialized: PIT channel 0, 100Hz\n");

    StIntP_Unmask(0x20);
}

static void init_rtc(void)
{
    uint8_t temp;

    StIoA_Out8(0x0070, 0x8B);
    temp = StIoA_In8(0x0071);
    StIoA_Out8(0x0070, 0x8B);
    StIoA_Out8(0x0071, temp & ~0x70);
}

static StStatus init_krt(void)
{
    StStatus status;
    size_t krt_size;

    krt_size = (uintptr_t)&_krt_end - (uintptr_t)&_krt_start;

    status = StMm_AllocateSparseTo(
        VMM_DOMAIN_KRT_VPN_BASE,
        ALIGN_DIV(krt_size, PAGE_SIZE),
        PMM_DEFAULT,
        MAP_USER
    );
    if (!CHECK_SUCCESS(status)) return status;

    memcpy(PAGE_TO_VPTR(VMM_DOMAIN_KRT_VPN_BASE), &_krt_start, krt_size);

    status = StMm_Remap(
        VMM_DOMAIN_KRT_VPN_BASE,
        ALIGN_DIV(krt_size, PAGE_SIZE),
        MAP_USER | MAP_READONLY
    );
    if (!CHECK_SUCCESS(status)) return status;

    return STATUS_SUCCESS;
}

__externally_visible void _pc_init(struct bootinfo_table_header *btblhdr)
{
    StStatus status;
    struct bootinfo_entry_header *enthdr = NULL;
    struct bootinfo_entry_command_args *caent = NULL;
    struct bootinfo_entry_memory_map *mment = NULL;
    struct bootinfo_entry_unavailable_frames *ufent = NULL;
    struct bootinfo_entry_pagetable_vpn *pvent = NULL;
    struct bootinfo_table_header *newbtblhdr = NULL;

    StLog_EarlyInit(early_print_char, NULL);

    LOG_INFO("Starting Strata...\n");

    enthdr = (void *)((uintptr_t)btblhdr + btblhdr->header_size);
    for (int i = 0; i < btblhdr->entry_count; i++) {
        switch (enthdr->type) {
        case BET_COMMAND_ARGS:
            caent = (void *)((uintptr_t)enthdr + enthdr->header_size);
            break;
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

#ifndef NDEBUG
    if (caent) {
        for (uint32_t i = 0; i < caent->arg_count; i++) {
            if (strcmp(&btblhdr->strtab[caent->arg_offsets[i]], "-v") == 0) {
                StLog_SetLevel(LL_DEBUG);
            }
        }
    }

#endif

    LOG_DEBUG("checking CPU features...\n");
    status = StA_CheckCpuFeatures();
    if (!CHECK_SUCCESS(status)) {
        St_Panic(status, "failed to check CPU features");
    }

    LOG_INFO("starting uptime counter...\n");
    StTimeP_StartUptime();

    LOG_INFO("initializing GDT...\n");
    StP_InitGdt();

    LOG_INFO("initializing MMU...\n");
    status = StMmuP_Init();
    if (!CHECK_SUCCESS(status)) {
        St_Panic(status, "failed to initialize mmu");
    }

    LOG_INFO("initializing PMA...\n");
    status = StPmm_Init();
    if (!CHECK_SUCCESS(status)) {
        St_Panic(status, "failed to initialize physical memory allocator");
    }

    /* mark usable frames first */
    for (uint32_t i = 0; i < mment->entry_count; i++) {
        if (mment->entries[i].type != BEMT_FREE) continue;

        status = StPmm_MarkUsableContiguousFrame(
            ADDR_TO_PAGE(ALIGN(mment->entries[i].base, PAGE_SIZE)),
            ADDR_TO_PAGE(mment->entries[i].base + mment->entries[i].size) - 1
        );
        if (!CHECK_SUCCESS(status)) {
            St_Panic(status, "failed to mark usable frames");
        }
    }

    /* mark unusable frames in case of there's overlapped unusable area inside a usable area */
    for (uint32_t i = 0; i < mment->entry_count; i++) {
        if (mment->entries[i].type == BEMT_FREE) continue;

        status = StPmm_MarkUnusableContiguousFrame(
            ADDR_TO_PAGE(mment->entries[i].base),
            ADDR_TO_PAGE(ALIGN(mment->entries[i].base + mment->entries[i].size, PAGE_SIZE)) - 1
        );
        if (!CHECK_SUCCESS(status)) {
            St_Panic(status, "failed to mark unusable frames");
        }
    }

    /* mark unusable frames */
    for (uint32_t i = 0; i < ufent->entry_count; i++) {
        /*
            page table is already migrated and replaced to a new one.
            so just allow allocation to this area.
        */
        if (ufent->entries[i].type == BEUT_PAGETABLE) continue;

        status = StPmm_MarkUnusableContiguousFrame(
            (St_VirtPage)ufent->entries[i].pfn_base,
            (St_VirtPage)(ufent->entries[i].pfn_base + ufent->entries[i].count - 1)
        );
        if (!CHECK_SUCCESS(status)) {
            St_Panic(status, "failed to mark unusable frames");
        }
    }

    /* mark trampoline area as unusable */
    status = StPmm_MarkUnusableContiguousFrame(
        VPTR_TO_PAGE(&_trampoline_load_),
        ADDR_TO_PAGE(
            ALIGN((uintptr_t)&_trampoline_load_ + (uintptr_t)&_trampoline_size_, PAGE_SIZE)
        ) - 1
    );
    if (!CHECK_SUCCESS(status)) {
        St_Panic(status, "failed to mark trampoline area as unusable");
    }

    /* finalize memory map and do late init */
    status = StPmm_LateInit();
    if (!CHECK_SUCCESS(status)) {
        St_Panic(status, "failed to late init physical memory manager");
    }

    LOG_INFO("initializing VMA...\n");
    status = StVmm_InitDomain(
        VMM_DOMAIN_KERNEL_FAST,
        ADDR_TO_PAGE(ALIGN((uintptr_t)&_end_, PAGE_SIZE)),
        VMM_DOMAIN_KERNEL_FAST_VPN_LIMIT
    );
    if (!CHECK_SUCCESS(status)) {
        St_Panic(status, "failed to initialize virtual memory allocator");
    }

    status = StVmm_InitDomain(
        VMM_DOMAIN_KERNEL_SLOW,
        VMM_DOMAIN_KERNEL_SLOW_VPN_BASE,
        VMM_DOMAIN_KERNEL_SLOW_VPN_LIMIT
    );
    if (!CHECK_SUCCESS(status)) {
        St_Panic(status, "failed to initialize virtual memory allocator");
    }

    status = StVmm_InitDomain(VMM_DOMAIN_IO, VMM_DOMAIN_IO_VPN_BASE, VMM_DOMAIN_IO_VPN_LIMIT);
    if (!CHECK_SUCCESS(status)) {
        St_Panic(status, "failed to initialize virtual memory allocator");
    }

    status = StVmm_InitDomain(
        VMM_DOMAIN_MODULE,
        VMM_DOMAIN_MODULE_VPN_BASE,
        VMM_DOMAIN_MODULE_VPN_LIMIT
    );
    if (!CHECK_SUCCESS(status)) {
        St_Panic(status, "failed to initialize virtual memory allocator");
    }

    status = StVmm_InitDomain(VMM_DOMAIN_USER, VMM_DOMAIN_USER_VPN_BASE, VMM_DOMAIN_USER_VPN_LIMIT);
    if (!CHECK_SUCCESS(status)) {
        St_Panic(status, "failed to initialize virtual memory allocator");
    }

    LOG_INFO("initializing memory manager...\n");
    status = StMm_Init();
    if (!CHECK_SUCCESS(status)) {
        St_Panic(status, "failed to initialize memory manager");
    }

    LOG_DEBUG("relocating bootinfo table...\n");
    newbtblhdr = malloc(btblhdr->size);
    if (!newbtblhdr) {
        St_Panic(STATUS_UNKNOWN_ERROR, "cannot allocate memory for bootinfo table");
    }

    memcpy(newbtblhdr, btblhdr, btblhdr->size);

    LOG_INFO("late initializing MMU...\n")
    status = StMmuP_LateInit();
    if (!CHECK_SUCCESS(status)) {
        St_Panic(status, "failed to late init MMU");
    }

    _pc_bootinfo_table = newbtblhdr;

    LOG_INFO("initializing ISRs...\n");
    status = StIntP_Init();
    if (!CHECK_SUCCESS(status)) {
        St_Panic(status, "failed to initialize interrupt system");
    }

    StPicP_Remap(0x20, 0x28);

    StInt_CreateHandler(0x20, NULL, pit_isr, NULL);
    StInt_CreateHandler(0x80, NULL, trap_isr, NULL);

    init_rtc();

    LOG_INFO("initializing PIT...\n");
    init_pit();

    LOG_INFO("activating common CPU features...\n");
    status = StA_ActivateCommonCpuFeatures();
    if (!CHECK_SUCCESS(status)) {
        St_Panic(status, "failed to activate common CPU features");
    }

    LOG_INFO("initializing CPU local data...\n");
    status = StCpuLocalP_Init();
    if (!CHECK_SUCCESS(status)) {
        St_Panic(status, "failed to initialize CPU local data");
    }

    LOG_INFO("initializing syscall handler...\n");
    status = StSyscallP_Init();
    if (!CHECK_SUCCESS(status)) {
        St_Panic(status, "failed to initialize syscall handler");
    }

    LOG_INFO("initializing KRT...\n");
    status = init_krt();
    if (!CHECK_SUCCESS(status)) {
        St_Panic(status, "failed to initialize KRT");
    }
}
