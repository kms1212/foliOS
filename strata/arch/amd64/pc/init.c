#include "config.h"
#include "strata/plat/interrupt_constants.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <uacpi/types.h>

#if STRATA_ENABLE_ACPI
#    include <uacpi/acpi.h>
#    include <uacpi/internal/tables.h>
#    include <uacpi/kernel_api.h>
#    include <uacpi/tables.h>
#    include <uacpi/uacpi.h>

#endif  // STRATA_ENABLE_ACPI

#include <strata/arch/apic.h>
#include <strata/arch/cpufeatures.h>
#include <strata/arch/interrupt.h>
#include <strata/arch/intrinsics/io.h>
#include <strata/arch/mmu.h>

#include <strata/plat/cpulocal.h>
#include <strata/plat/gdt.h>
#include <strata/plat/hpet.h>
#include <strata/plat/interrupt.h>
#include <strata/plat/ioapic.h>
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
#include <strata/mm/pmm.h>
#include <strata/mm/pool.h>
#include <strata/mm/types.h>
#include <strata/mm/vmm.h>
#include <strata/panic.h>
#include <strata/scheduler.h>
#include <strata/status.h>
#include <strata/thread.h>

#include <loadst/bootinfo.h>

#define MODULE_NAME "init"

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

    struct acpi_fadt *fadt;
    uacpi_status = uacpi_table_fadt(&fadt);
    if (uacpi_unlikely_error(uacpi_status)) {
        return MAKE_UACPI_STATUS(uacpi_status);
    }

    return STATUS_SUCCESS;
}

static uacpi_iteration_decision iterate_ioapic_entry(
    uacpi_handle data, struct acpi_entry_hdr *entry
)
{
    StStatus *status_out = (StStatus *)data;
    StStatus status;
    struct acpi_madt_ioapic *ioapic;

    if (entry->type != ACPI_MADT_ENTRY_TYPE_IOAPIC) return UACPI_ITERATION_DECISION_CONTINUE;

    ioapic = (struct acpi_madt_ioapic *)entry;
    status = StIoapicP_Add(ioapic->id, ioapic->gsi_base, ioapic->address);
    if (!CHECK_SUCCESS(status)) {
        *status_out = status;
        return UACPI_ITERATION_DECISION_BREAK;
    }

    LOG_INFO(
        LM_CAT_ACPI,
        "added IOAPIC: ID=%d, GSI=%d, MMIO=0x%lx\n",
        ioapic->id,
        ioapic->gsi_base,
        (uintptr_t)ioapic->address
    );

    return UACPI_ITERATION_DECISION_CONTINUE;
}

struct iso_iter_data {
    StStatus status;
    uint32_t redirected_gsi_bitmap;
    uint16_t redirected_legacy_irq_bitmap;
};

static uacpi_iteration_decision iterate_iso_entry(uacpi_handle data, struct acpi_entry_hdr *entry)
{
    struct iso_iter_data *iter_data = (struct iso_iter_data *)data;
    StStatus status;
    struct acpi_madt_interrupt_source_override *iso;
    uint16_t flags = DLV_MODE_NORMAL | RFLAGS_INT_MASK;

    if (entry->type != ACPI_MADT_ENTRY_TYPE_INTERRUPT_SOURCE_OVERRIDE) {
        return UACPI_ITERATION_DECISION_CONTINUE;
    }

    iso = (struct acpi_madt_interrupt_source_override *)entry;

    if ((iso->flags & ACPI_MADT_POLARITY_MASK) == ACPI_MADT_POLARITY_ACTIVE_LOW) {
        flags |= RFLAGS_ACTIVE_LOW;
    }
    if ((iso->flags & ACPI_MADT_TRIGGERING_MASK) == ACPI_MADT_TRIGGERING_LEVEL) {
        flags |= RFLAGS_LEVEL_TRIGGERED;
    }

    status = StIoapicP_RouteGsiToVector(iso->gsi, 0x20 + iso->source, 0, flags);
    if (!CHECK_SUCCESS(status)) {
        iter_data->status = status;
        return UACPI_ITERATION_DECISION_BREAK;
    }

    iter_data->redirected_legacy_irq_bitmap |= (1 << iso->source);

    if (iso->gsi < 32) {
        iter_data->redirected_gsi_bitmap |= (1 << iso->gsi);
    }

    return UACPI_ITERATION_DECISION_CONTINUE;
}

static StStatus init_apic(void)
{
    StStatus status;
    uacpi_status uacpi_status;
    struct uacpi_table table;
    struct iso_iter_data iso_iter_data = {
        0,
    };

    if (!g_p_cpu_features->has_apic) return STATUS_NOT_SUPPORTED;

    /* disable PIC */
    StPicP_Disable();

    /* enable APIC & LAPIC */
    status = StApicA_EnableGlobal();
    if (!CHECK_SUCCESS(status)) return status;

    status = StApicA_EnableLocal();
    if (!CHECK_SUCCESS(status)) return status;

    /* query MADT */
    uacpi_status = uacpi_table_find_by_signature(ACPI_MADT_SIGNATURE, &table);
    if (uacpi_unlikely_error(uacpi_status)) {
        LOG_ERROR(
            LM_CAT_UNCLASSIFIED,
            "could not find ACPI MADT: %s\n",
            uacpi_status_to_string(uacpi_status)
        );
        return MAKE_UACPI_STATUS(uacpi_status);
    }

    /* register I/O APIC */
    uacpi_status =
        uacpi_for_each_subtable(table.hdr, sizeof(struct acpi_madt), iterate_ioapic_entry, &status);
    if (uacpi_unlikely_error(uacpi_status)) {
        LOG_ERROR(
            LM_CAT_UNCLASSIFIED,
            "an error occured while iterating I/O APIC entries in MADT: %s\n",
            uacpi_status_to_string(uacpi_status)
        );
        return MAKE_UACPI_STATUS(uacpi_status);
    }
    if (!CHECK_SUCCESS(status)) return status;

    /* override interrupt sources */
    uacpi_status = uacpi_for_each_subtable(
        table.hdr,
        sizeof(struct acpi_madt),
        iterate_iso_entry,
        &iso_iter_data
    );
    if (uacpi_unlikely_error(uacpi_status)) {
        LOG_ERROR(
            LM_CAT_UNCLASSIFIED,
            "an error occured while iterating interrupt source override entries in MADT: %s\n",
            uacpi_status_to_string(uacpi_status)
        );
        return MAKE_UACPI_STATUS(uacpi_status);
    }
    if (!CHECK_SUCCESS(iso_iter_data.status)) return iso_iter_data.status;

    for (int i = 0; i < 16; i++) {
        if (iso_iter_data.redirected_legacy_irq_bitmap & (1 << i)) continue;
        if (iso_iter_data.redirected_gsi_bitmap & (1 << i)) continue;

        status = StIoapicP_RouteGsiToVector(i, 0x20 + i, 0, DLV_MODE_NORMAL | RFLAGS_INT_MASK);
        if (!CHECK_SUCCESS(status)) return status;
    }

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
    struct StThread *current_thread;
    struct StThread *next_thread;
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

#if STRATA_ENABLE_ACPI
static void dump_acpi(void)
{
    struct uacpi_table table;
    struct acpi_fadt *fadt;
    struct acpi_madt *madt;
    struct acpi_hpet *hpet;

    if (!uacpi_table_fadt(&fadt)) {
        LOG_DEBUG(LM_CAT_ACPI, "FADT: 0x%p\n", (void *)fadt);

        LOG_DEBUG(LM_CAT_ACPI, "\tPreferred PM Profile: %d\n", fadt->preferred_pm_profile);
        LOG_DEBUG(LM_CAT_ACPI, "\tSCI Interrupt: %d\n", fadt->sci_int);
        LOG_DEBUG(LM_CAT_ACPI, "\tSMI Command Port: 0x%08" PRIX32 "\n", fadt->smi_cmd);

        LOG_DEBUG(LM_CAT_ACPI, "\tACPI Enable Port: %02X\n", fadt->acpi_enable);
        LOG_DEBUG(LM_CAT_ACPI, "\tACPI Disable Port: %02X\n", fadt->acpi_disable);

        LOG_DEBUG(
            LM_CAT_ACPI,
            "\tPM1: evt_len=%d, ctl_len=%d\n",
            fadt->pm1_evt_len,
            fadt->pm1_cnt_len
        );
        LOG_DEBUG(
            LM_CAT_ACPI,
            "\tPM1a: evt_blk=0x%08" PRIX32 ", ctl_blk=0x%08" PRIX32 "\n",
            fadt->pm1a_evt_blk,
            fadt->pm1a_cnt_blk
        );
        LOG_DEBUG(
            LM_CAT_ACPI,
            "\tPM1b: evt_blk=0x%08" PRIX32 ", ctl_blk=0x%08" PRIX32 "\n",
            fadt->pm1b_evt_blk,
            fadt->pm1b_cnt_blk
        );

        LOG_DEBUG(
            LM_CAT_ACPI,
            "\tPM2: ctl_blk=0x%08" PRIX32 ", ctl_len=%d\n",
            fadt->pm2_cnt_blk,
            fadt->pm2_cnt_len
        );

        LOG_DEBUG(
            LM_CAT_ACPI,
            "\tPM Timer: block=0x%08" PRIX32 ", length=%d\n",
            fadt->pm_tmr_blk,
            fadt->pm_tmr_len
        );

        LOG_DEBUG(
            LM_CAT_ACPI,
            "\tGPE0: block=0x%08" PRIX32 ", length=%d\n",
            fadt->gpe0_blk,
            fadt->gpe0_blk_len
        );
        LOG_DEBUG(
            LM_CAT_ACPI,
            "\tGPE1: base=%d, block=0x%08" PRIX32 ", length=%d\n",
            fadt->gpe1_base,
            fadt->gpe1_blk,
            fadt->gpe1_blk_len
        );

        LOG_DEBUG(LM_CAT_ACPI, "\tCSTATE Control: %02X\n", fadt->cst_cnt);
        LOG_DEBUG(
            LM_CAT_ACPI,
            "\tWorst Latency: c2=%u c3=%u\n",
            fadt->p_lvl2_lat,
            fadt->p_lvl3_lat
        );
        LOG_DEBUG(
            LM_CAT_ACPI,
            "\tCPU Cache Flush: size=%u, stride=%u\n",
            fadt->flush_size,
            fadt->flush_stride
        );
        LOG_DEBUG(
            LM_CAT_ACPI,
            "\tDuty Cycle Setting: offset=%u, width=%u\n",
            fadt->duty_offset,
            fadt->duty_width
        );
        LOG_DEBUG(
            LM_CAT_ACPI,
            "\tRTC Alarm Offset: day=%u, month=%u\n",
            fadt->day_alrm,
            fadt->mon_alrm
        );
        LOG_DEBUG(LM_CAT_ACPI, "\tRTC Century Offset: %u\n", fadt->century);

        if (fadt->hdr.revision >= 3) {
            LOG_DEBUG(LM_CAT_ACPI, "\tIA-PC Boot Architecture Flags: %04X\n", fadt->iapc_boot_arch);
        }
    }

    if (!uacpi_table_find_by_signature(ACPI_MADT_SIGNATURE, &table)) {
        madt = table.ptr;
        LOG_DEBUG(LM_CAT_ACPI, "MADT: 0x%p\n", (void *)madt);

        LOG_DEBUG(
            LM_CAT_ACPI,
            "\tLocal APIC Address: 0x%08" PRIX32 "\n",
            madt->local_interrupt_controller_address
        );
        LOG_DEBUG(LM_CAT_ACPI, "\t8259 PIC Installed: %s\n", (madt->flags & 1) ? "true" : "false");

        union {
            struct acpi_entry_hdr header;
            struct acpi_madt_lapic lapic;
            struct acpi_madt_ioapic ioapic;
            struct acpi_madt_interrupt_source_override interrupt_source_override;
            struct acpi_madt_nmi_source nmi_src;
            struct acpi_madt_lapic_nmi lapic_nmi;
            struct acpi_madt_lapic_address_override lapic_address_override;
            struct acpi_madt_x2apic x2apic;
        } *entry = (void *)((uint8_t *)madt + sizeof(*madt));

        while ((uintptr_t)((ptrdiff_t)entry - (ptrdiff_t)madt) < madt->hdr.length) {
            switch (entry->header.type) {
            case ACPI_MADT_ENTRY_TYPE_LAPIC:
                LOG_DEBUG(LM_CAT_ACPI, "\tProcessor Local APIC Entry:\n");
                LOG_DEBUG(LM_CAT_ACPI, "\t\tACPI Processor ID: 0x%02X\n", entry->lapic.uid);
                LOG_DEBUG(LM_CAT_ACPI, "\t\tLAPIC ID: 0x%02X\n", entry->lapic.id);
                LOG_DEBUG(LM_CAT_ACPI, "\t\tFlags: 0x%08" PRIX32 "\n", entry->lapic.flags);
                break;
            case ACPI_MADT_ENTRY_TYPE_IOAPIC:
                LOG_DEBUG(LM_CAT_ACPI, "\tI/O APIC Entry:\n");
                LOG_DEBUG(LM_CAT_ACPI, "\t\tID: 0x%02X\n", entry->ioapic.id);
                LOG_DEBUG(LM_CAT_ACPI, "\t\tAddress: 0x%08" PRIX32 "\n", entry->ioapic.address);
                LOG_DEBUG(
                    LM_CAT_ACPI,
                    "\t\tGlobal System Interrupt Base: 0x%08" PRIX32 "\n",
                    entry->ioapic.gsi_base
                );
                break;
            case ACPI_MADT_ENTRY_TYPE_INTERRUPT_SOURCE_OVERRIDE:
                LOG_DEBUG(LM_CAT_ACPI, "\tInterrupt Source Override Entry:\n");
                LOG_DEBUG(LM_CAT_ACPI, "\t\tBus: 0x%02X\n", entry->interrupt_source_override.bus);
                LOG_DEBUG(
                    LM_CAT_ACPI,
                    "\t\tIRQ: 0x%02X\n",
                    entry->interrupt_source_override.source
                );
                LOG_DEBUG(
                    LM_CAT_ACPI,
                    "\t\tGlobal System Interrupt: 0x%08" PRIX32 "\n",
                    entry->interrupt_source_override.gsi
                );
                LOG_DEBUG(
                    LM_CAT_ACPI,
                    "\t\tFlags: 0x%04X\n",
                    entry->interrupt_source_override.flags
                );
                break;
            case ACPI_MADT_ENTRY_TYPE_NMI_SOURCE:
                LOG_DEBUG(LM_CAT_ACPI, "\tNMI Source Entry:\n");
                break;
            case ACPI_MADT_ENTRY_TYPE_LAPIC_NMI:
                LOG_DEBUG(LM_CAT_ACPI, "\tLocal APIC NMI Entry:\n");
                LOG_DEBUG(LM_CAT_ACPI, "\t\tACPI Processor ID: 0x%02X\n", entry->lapic_nmi.uid);
                LOG_DEBUG(LM_CAT_ACPI, "\t\tFlags: 0x%04X\n", entry->lapic_nmi.flags);
                LOG_DEBUG(LM_CAT_ACPI, "\t\tLocal APIC LINT#: 0x%02X\n", entry->lapic_nmi.lint);
                break;
            case ACPI_MADT_ENTRY_TYPE_LAPIC_ADDRESS_OVERRIDE:
                LOG_DEBUG(LM_CAT_ACPI, "\tLocal APIC Address Override Entry:\n");
                LOG_DEBUG(
                    LM_CAT_ACPI,
                    "\t\tAddress: 0x%016" PRIX64 "\n",
                    entry->lapic_address_override.address
                );
                break;
            default:
                break;
            }

            entry = (void *)((uint8_t *)entry + entry->header.length);
        }
    }

    if (!uacpi_table_find_by_signature(ACPI_HPET_SIGNATURE, &table)) {
        hpet = table.ptr;
        LOG_DEBUG(LM_CAT_ACPI, "HPET: 0x%p\n", (void *)hpet);

        LOG_DEBUG(LM_CAT_ACPI, "\tBlock ID: %08" PRIX32 "\n", hpet->block_id);
        LOG_DEBUG(
            LM_CAT_ACPI,
            "\tAddress: asp=%u width=%u offset=%u asz=%u address=%016" PRIX64 "\n",
            hpet->address.address_space_id,
            hpet->address.register_bit_width,
            hpet->address.register_bit_offset,
            hpet->address.access_size,
            hpet->address.address
        );
        LOG_DEBUG(LM_CAT_ACPI, "\tNumber: %u\n", hpet->number);
        LOG_DEBUG(LM_CAT_ACPI, "\tMinimum Clock Tick: %u\n", hpet->min_clock_tick);
        LOG_DEBUG(LM_CAT_ACPI, "\tFlags: %02X\n", hpet->flags);
    }
}

#endif  // STRATA_ENABLE_ACPI

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
    int use_acpi = STRATA_ENABLE_ACPI;
    int use_apm = STRATA_ENABLE_APM;

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

#ifdef NDEBUG
    if (caent) {
        for (uint32_t i = 0; i < caent->arg_count; i++) {
            if (strcmp(&btblhdr->strtab[caent->arg_offsets[i]], "-v") == 0) {
                StLog_SetLevel(LL_DEBUG);
            }
        }
    }

#endif

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
        use_acpi = 0;
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

    LOG_INFO(LM_CAT_UNCLASSIFIED, "initializing RTC...\n");
    init_rtc();

    LOG_INFO(LM_CAT_UNCLASSIFIED, "initializing timer...\n");
    StTimeP_InitTimer(use_hpet);

    LOG_INFO(LM_CAT_UNCLASSIFIED, "initializing preemption handler...\n");
    StInt_CreateHandler(
        use_apic ? LAPIC_TIMER_IRQ_VECTOR : (use_hpet ? HPET_IRQ_VECTOR : LEGACY_IRQ_VECTOR_BASE),
        NULL,
        preempt_isr,
        NULL
    );

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

#if STRATA_ENABLE_ACPI
    dump_acpi();

#endif  // STRATA_ENABLE_ACPI
}
