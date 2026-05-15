#include "config.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if STRATA_ENABLE_ACPI
#    include <uacpi/status.h>
#    include <uacpi/uacpi.h>

#endif  // STRATA_ENABLE_ACPI

#include <strata/arch/apic.h>
#include <strata/arch/cpufeatures.h>
#include <strata/arch/interrupt.h>
#include <strata/arch/intrinsics/io.h>
#include <strata/arch/intrinsics/register.h>
#include <strata/arch/mmu_constants.h>

#include <strata/plat/cpulocal.h>
#include <strata/plat/gdt.h>
#include <strata/plat/hpet.h>
#include <strata/plat/interrupt.h>
#include <strata/plat/interrupt_constants.h>
#include <strata/plat/memmap.h>
#include <strata/plat/mm.h>
#include <strata/plat/pic.h>
#include <strata/plat/syscall.h>
#include <strata/plat/thread.h>
#include <strata/plat/time.h>

#include <strata/compiler.h>
#include <strata/interrupt.h>
#include <strata/log.h>
#include <strata/macros.h>
#include <strata/mm.h>
#include <strata/mm/address_space_refs.h>
#include <strata/mm/pmm.h>
#include <strata/mm/pool.h>
#include <strata/mm/types.h>
#include <strata/mm/vmm.h>
#include <strata/panic.h>
#include <strata/scheduler.h>
#include <strata/status.h>
#include <strata/thread.h>
#include <strata/thread_refs.h>

#include <loadst/bootinfo.h>

#define MODULE_NAME "init"
#define PAGE_FAULT_VECTOR 0x0E

__externally_visible struct bootinfo_table_header *_pc_bootinfo_table;

extern char __trampoline_load[];

extern char _krt_start[];
extern char _krt_end[];

extern void (*__init_array_start[])(void);
extern void (*__init_array_end[])(void);

extern char __end[];

size_t _trampoline_runtime_size;

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

#if STRATA_ENABLE_ACPI
#    define MAKE_UACPI_STATUS(uacpi_status)                                                        \
        ((uacpi_status)                                                                            \
             ? MAKE_STATUS(MAKE_BASE_STATUS(1, uacpi_status, STATUS_AREA_ACPI), STATUS_ATTR_NONE)  \
             : STATUS_SUCCESS)

static StStatus init_acpi(void)
{
    uacpi_status uacpi_status;
    static uint8_t acpi_buf[PAGE_SIZE];

    uacpi_status = uacpi_setup_early_table_access(acpi_buf, sizeof(acpi_buf));
    if (uacpi_unlikely_error(uacpi_status)) {
        LOG_ERROR(
            LM_CAT_UNCLASSIFIED,
            "uACPI initialization failed: %s\n",
            uacpi_status_to_string(uacpi_status)
        );
        return MAKE_UACPI_STATUS(uacpi_status);
    }

    return STATUS_SUCCESS;
}

static StStatus init_apic(void)
{
    StStatus status;

    if (!g_p_cpu_features->has_apic) return STATUS_NOT_SUPPORTED;

    /* disable PIC */
    StPicP_Disable();

    /* enable APIC & LAPIC */
    status = StApicA_EnableGlobal();
    if (!CHECK_SUCCESS(status)) return status;

    status = StApicA_EnableLocal();
    if (!CHECK_SUCCESS(status)) return status;

    return STATUS_SUCCESS;
}

#endif  // STRATA_ENABLE_ACPI

static void init_rtc(void)
{
    uint8_t temp;

    StIoA_Out8(0x0070, 0x8B);
    temp = StIoA_In8(0x0071);
    StIoA_Out8(0x0070, 0x8B);
    StIoA_Out8(0x0071, temp & ~0x70);
}

static void *preempt_isr(
    int num, struct StA_InterruptFrame *frame, struct StIntP_Context *ctx, void *data
)
{
    StStatus status;
    StThread_InternalRef current_thread;
    StThread_InternalRef next_thread;
    void *next_stack_ptr;

    if (StThread_IsPreemptionEnabled()) {
        if (StScheduler_ShouldMaintain()) {
            status = StScheduler_Maintain();
            if (!CHECK_SUCCESS(status)) return NULL;
        }

        status = StScheduler_GetNextThread(&next_thread);
        if (!CHECK_SUCCESS(status) || !next_thread) return NULL;

        status = StScheduler_GetCurrentThread(&current_thread);
        if (!CHECK_SUCCESS(status)) return NULL;

        if (next_thread == current_thread) {
            return NULL;
        }

        status = StThreadP_Switch(next_thread, ctx, &next_stack_ptr);
        if (!CHECK_SUCCESS(status)) return NULL;

        return next_stack_ptr;
    }

    return NULL;
}

static void *page_fault_isr(
    int num, struct StA_InterruptFrame *frame, struct StIntP_Context *ctx, void *data
)
{
    StStatus status;
    uint64_t fault_addr;
    struct StCpuLocalP_Data *cpu_data;
    StAddressSpace_StrongRef asp;
    void *next_stack_ptr;

    (void)num;
    (void)data;

    fault_addr = StA_ReadCr2();
    cpu_data = StCpuLocalP_GetData();
    asp = cpu_data ? (StAddressSpace_StrongRef)cpu_data->current_asp : NULL;

    status = StMm_HandlePageFault(asp, (uintptr_t)fault_addr, frame->error);
    if (CHECK_SUCCESS(status)) return NULL;

    next_stack_ptr = StIntP_HandleUserFault(num, status, 1, (uintptr_t)fault_addr, frame, ctx);
    if (next_stack_ptr) return next_stack_ptr;

    St_PanicFromContext(
        status,
        ctx->rbp,
        frame->rip,
        "Unhandled page fault at 0x%016" PRIX64 " error=0x%08" PRIX64
        " rip=0x%04X:0x%016" PRIX64 "\n",
        fault_addr,
        frame->error,
        frame->cs,
        frame->rip
    );

    return NULL;
}

static StStatus init_krt(void)
{
    StStatus status;
    size_t krt_size;

    krt_size = (uintptr_t)&_krt_end - (uintptr_t)&_krt_start;

    status = StMm_AllocateGlobalSparseTo(
        VMM_DOMAIN_KRT_GLOBAL,
        MEMMAP_KRT_VPN_BASE,
        ALIGN_DIV(krt_size, PAGE_SIZE),
        NULL,
        AF_DEFAULT,
        MF_USER_DEFAULT
    );
    if (!CHECK_SUCCESS(status)) return status;

    memcpy(PAGE_TO_VPTR(MEMMAP_KRT_VPN_BASE), &_krt_start, krt_size);

    status = StMm_SetGlobalPageFlags(
        MEMMAP_KRT_VPN_BASE,
        ALIGN_DIV(krt_size, PAGE_SIZE),
        MF_USER_DEFAULT & ~MF_WRITABLE
    );
    if (!CHECK_SUCCESS(status)) return status;

    return STATUS_SUCCESS;
}

// NOLINTNEXTLINE(readability-identifier-naming)
__externally_visible void _pc_init(struct bootinfo_table_header *btblhdr)
{
    StStatus status;
    struct bootinfo_entry_header *enthdr = NULL;
    struct bootinfo_entry_command_args *caent = NULL;
    struct bootinfo_entry_memory_map *mment = NULL;
    struct bootinfo_entry_unavailable_frames *ufent = NULL;
    struct bootinfo_entry_pagetable_vpn *pvent = NULL;
    struct bootinfo_table_header *newbtblhdr = NULL;
    int use_apic = STRATA_ENABLE_ACPI;
    int use_hpet = STRATA_ENABLE_ACPI;
    // int use_acpi = STRATA_ENABLE_ACPI;
    // int use_apm = STRATA_ENABLE_APM;

    StLog_EarlyInit(early_print_char, NULL);

    LOG_INFO(LM_CAT_UNCLASSIFIED, "Starting Strata...\n");

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

    if (caent) {
        for (uint32_t i = 0; i < caent->arg_count; i++) {
#ifdef NDEBUG
            if (strcmp(&btblhdr->strtab[caent->arg_offsets[i]], "-v") == 0) {
                StLog_SetLevel(LL_DEBUG);
            }

#else
            if (strcmp(&btblhdr->strtab[caent->arg_offsets[i]], "-v") == 0) {
                StLog_SetLevel(LL_TRACE);
            }

#endif
        }
    }

    // TODO: parse arguments

    LOG_DEBUG(LM_CAT_UNCLASSIFIED, "checking CPU features...\n");
    status = StA_CheckCpuFeatures();
    if (!CHECK_SUCCESS(status)) {
        St_Panic(status, "failed to check CPU features");
    }

    LOG_INFO(LM_CAT_UNCLASSIFIED, "activating common CPU features...\n");
    status = StA_ActivateCommonCpuFeatures();
    if (!CHECK_SUCCESS(status)) {
        St_Panic(status, "failed to activate common CPU features");
    }

    LOG_INFO(LM_CAT_UNCLASSIFIED, "initializing FPU/SIMD state...\n");
    status = StThreadP_InitializeFpuSimdState();
    if (!CHECK_SUCCESS(status) && status != STATUS_ALREADY_PERFORMED) {
        St_Panic(status, "failed to initialize FPU/SIMD state");
    }

    LOG_INFO(LM_CAT_UNCLASSIFIED, "initializing GDT...\n");
    StP_InitGdt();

    LOG_INFO(LM_CAT_UNCLASSIFIED, "initializing CPU local data...\n");
    status = StCpuLocalP_Init();
    if (!CHECK_SUCCESS(status)) {
        St_Panic(status, "failed to initialize CPU local data");
    }

    LOG_INFO(LM_CAT_UNCLASSIFIED, "initializing memory manager...\n");
    status = StMm_Init();
    if (!CHECK_SUCCESS(status)) {
        St_Panic(status, "failed to initialize memory manager");
    }

    LOG_INFO(LM_CAT_UNCLASSIFIED, "initializing PMA...\n");
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
        VPTR_TO_PAGE(__trampoline_load),
        ADDR_TO_PAGE(ALIGN((uintptr_t)__trampoline_load + _trampoline_runtime_size, PAGE_SIZE)) - 1
    );
    if (!CHECK_SUCCESS(status)) {
        St_Panic(status, "failed to mark trampoline area as unusable");
    }

    /* finalize memory map and do late init */
    status = StPmm_LateInit();
    if (!CHECK_SUCCESS(status)) {
        St_Panic(status, "failed to late init physical memory manager");
    }

    LOG_INFO(LM_CAT_UNCLASSIFIED, "initializing VMA...\n");
    status = StVmm_InitGlobalDomain(
        VMM_DOMAIN_KERNEL_FAST,
        ADDR_TO_PAGE(ALIGN((uintptr_t)__end, PAGE_SIZE)),
        MEMMAP_KERNEL_FAST_VPN_LIMIT
    );
    if (!CHECK_SUCCESS(status)) {
        St_Panic(status, "failed to initialize virtual memory allocator");
    }

    status = StVmm_InitGlobalDomain(
        VMM_DOMAIN_KERNEL_SLOW,
        MEMMAP_KERNEL_SLOW_VPN_BASE,
        MEMMAP_KERNEL_SLOW_VPN_LIMIT
    );
    if (!CHECK_SUCCESS(status)) {
        St_Panic(status, "failed to initialize virtual memory allocator");
    }

    status = StVmm_InitGlobalDomain(VMM_DOMAIN_IO, MEMMAP_IO_VPN_BASE, MEMMAP_IO_VPN_LIMIT);
    if (!CHECK_SUCCESS(status)) {
        St_Panic(status, "failed to initialize virtual memory allocator");
    }

    status =
        StVmm_InitGlobalDomain(VMM_DOMAIN_MODULE, MEMMAP_MODULE_VPN_BASE, MEMMAP_MODULE_VPN_LIMIT);
    if (!CHECK_SUCCESS(status)) {
        St_Panic(status, "failed to initialize virtual memory allocator");
    }

    status =
        StVmm_InitGlobalDomain(VMM_DOMAIN_KRT_GLOBAL, MEMMAP_KRT_VPN_BASE, MEMMAP_KRT_VPN_LIMIT);
    if (!CHECK_SUCCESS(status)) {
        St_Panic(status, "failed to initialize virtual memory allocator");
    }

    LOG_DEBUG(LM_CAT_UNCLASSIFIED, "relocating bootinfo table...\n");
    status = StPool_Allocate(btblhdr->size, (void **)&newbtblhdr);
    if (!CHECK_SUCCESS(status)) {
        St_Panic(status, "cannot allocate memory for bootinfo table");
    }

    memcpy(newbtblhdr, btblhdr, btblhdr->size);

    LOG_INFO(LM_CAT_UNCLASSIFIED, "late initializing MMU...\n")
    status = StMmP_CleanupTempMapping();
    if (!CHECK_SUCCESS(status)) {
        St_Panic(status, "failed to late init MMU");
    }

    _pc_bootinfo_table = newbtblhdr;

#if STRATA_ENABLE_ACPI
    LOG_INFO(LM_CAT_UNCLASSIFIED, "initializing ACPI...\n");
    status = init_acpi();
    if (!CHECK_SUCCESS(status)) {
        LOG_ERROR(LM_CAT_UNCLASSIFIED, "failed to initialize ACPI\n");
        // use_acpi = 0;
        use_apic = 0;
        use_hpet = 0;
    }

    if (use_hpet) {
        status = StHpetP_Init();
        if (!CHECK_SUCCESS(status)) {
            LOG_ERROR(LM_CAT_UNCLASSIFIED, "failed to initialize HPET\n");
            use_hpet = 0;
        }
    }

    if (use_apic) {
        status = init_apic();
        if (!CHECK_SUCCESS(status)) {
            LOG_ERROR(LM_CAT_UNCLASSIFIED, "failed to initialize APIC\n");
            use_apic = 0;
        }
    }

    if (use_apic) {
        status = StApicA_InitLapicTimer();
        if (!CHECK_SUCCESS(status)) {
            LOG_ERROR(LM_CAT_UNCLASSIFIED, "failed to initialize lapic timer\n");
            use_apic = 0;
        }
    }

    if (use_apic) {
        status = StApicA_SetLapicTimerPeriodic(STRATA_TICK_RATE_HZ);
        if (!CHECK_SUCCESS(status)) {
            LOG_ERROR(LM_CAT_UNCLASSIFIED, "failed to initialize lapic timer\n");
            use_apic = 0;
        }
    }

#endif  // STRATA_ENABLE_ACPI

    LOG_INFO(LM_CAT_UNCLASSIFIED, "initializing interrupt system...\n");
    status = StIntP_Init(use_apic);
    if (!CHECK_SUCCESS(status)) {
        St_Panic(status, "failed to initialize interrupt system");
    }

    LOG_INFO(LM_CAT_UNCLASSIFIED, "initializing page fault handler...\n");
    status = StInt_CreateHandler(PAGE_FAULT_VECTOR, NULL, page_fault_isr, NULL);
    if (!CHECK_SUCCESS(status)) {
        St_Panic(status, "failed to create page fault handler");
    }

    LOG_INFO(LM_CAT_UNCLASSIFIED, "initializing RTC...\n");
    init_rtc();

    LOG_INFO(LM_CAT_UNCLASSIFIED, "initializing timer...\n");
    StTimeP_InitTimer(use_hpet);

    LOG_INFO(LM_CAT_UNCLASSIFIED, "initializing preemption handler...\n");
    if (use_apic) {
        status = StInt_CreateHandler(LAPIC_TIMER_IRQ_VECTOR, NULL, preempt_isr, NULL);
        if (!CHECK_SUCCESS(status)) {
            St_Panic(status, "failed to create preemption interrupt handler");
        }
    } else {
        status = StInt_CreateHandler(
            TIMER_IRQ_VECTOR(use_hpet),
            NULL,
            preempt_isr,
            NULL
        );
        if (!CHECK_SUCCESS(status)) {
            St_Panic(status, "failed to create preemption interrupt handler");
        }
    }

    LOG_INFO(LM_CAT_UNCLASSIFIED, "initializing syscall handler...\n");
    status = StSyscallA_Init();
    if (!CHECK_SUCCESS(status)) {
        St_Panic(status, "failed to initialize syscall handler");
    }

    LOG_INFO(LM_CAT_UNCLASSIFIED, "initializing KRT...\n");
    status = init_krt();
    if (!CHECK_SUCCESS(status)) {
        St_Panic(status, "failed to initialize KRT");
    }

    LOG_DEBUG(LM_CAT_UNCLASSIFIED, "running constructors...\n");
    for (int i = 0; &__init_array_start[i] != __init_array_end; i++) {
        LOG_INFO(
            LM_CAT_UNCLASSIFIED,
            "running constructor %p\n",
            (void *)(uintptr_t)__init_array_start[i]
        );
        __init_array_start[i]();
    }
}
