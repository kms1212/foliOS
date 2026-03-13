#include <vellum/shell.h>

#include <inttypes.h>
#include <stdio.h>

#include <uacpi/acpi.h>
#include <uacpi/kernel_api.h>
#include <uacpi/tables.h>

static int acpi_handler(struct shell_instance *inst, int argc, char **argv)
{
    /* List ACPI Info */
    struct acpi_rsdp *rsdp;
    if (uacpi_kernel_get_rsdp((uacpi_phys_addr *)&rsdp)) {
        return 1;
    }

    printf("RSDP found at 0x%p\n", (void *)rsdp);
    printf("\tVersion: %d", rsdp->revision);
    switch (rsdp->revision) {
    case 0:
        printf(" ACPI 1.0\n");
        break;
    case 2:
        printf(" ACPI 2.0 or upper\n");
        break;
    default:
        printf(" Unknown\n");
        break;
    }
    printf("\tOEM ID: %s\n", rsdp->oemid);

    struct acpi_rsdt *rsdt = (struct acpi_rsdt *)rsdp->rsdt_addr;
    printf("\tRSDT: 0x%p\n", (void *)rsdt);
    printf("\t\tCreator ID: %4s\n", (const char *)&rsdt->hdr.creator_id);
    printf("\t\tCreator Revision: 0x%08" PRIX32 "\n", rsdt->hdr.creator_revision);
    printf("\t\tLength: 0x%08" PRIX32 "\n", rsdt->hdr.length);

    if (rsdp->revision >= 2) {
        printf("\tXSDT: 0x%016llX\n", rsdp->xsdt_addr);
    }

    printf("\tTables: \n");
    for (unsigned int i = 0; i < (rsdt->hdr.length - sizeof(rsdt->hdr)) / sizeof(uint32_t); i++) {
        struct acpi_sdt_hdr *header = (struct acpi_sdt_hdr *)(rsdt->entries[i]);
        printf("\t- %.4s: 0x%p\n", header->signature, (void *)header);
    }

    struct acpi_fadt *fadt;
    if (!uacpi_table_fadt(&fadt)) {
        printf("FADT: 0x%p\n", (void *)fadt);

        printf("\tPreferred PM Profile: %d\n", fadt->preferred_pm_profile);
        printf("\tSCI Interrupt: %d\n", fadt->sci_int);
        printf("\tSMI Command Port: 0x%08" PRIX32 "\n", fadt->smi_cmd);

        printf("\tACPI Enable Port: %02X\n", fadt->acpi_enable);
        printf("\tACPI Disable Port: %02X\n", fadt->acpi_disable);

        printf("\tPM1: evt_len=%d, ctl_len=%d\n", fadt->pm1_evt_len, fadt->pm1_cnt_len);
        printf(
            "\tPM1a: evt_blk=0x%08" PRIX32 ", ctl_blk=0x%08" PRIX32 "\n",
            fadt->pm1a_evt_blk,
            fadt->pm1a_cnt_blk
        );
        printf(
            "\tPM1b: evt_blk=0x%08" PRIX32 ", ctl_blk=0x%08" PRIX32 "\n",
            fadt->pm1b_evt_blk,
            fadt->pm1b_cnt_blk
        );

        printf(
            "\tPM2: ctl_blk=0x%08" PRIX32 ", ctl_len=%d\n",
            fadt->pm2_cnt_blk,
            fadt->pm2_cnt_len
        );

        printf(
            "\tPM Timer: block=0x%08" PRIX32 ", length=%d\n",
            fadt->pm_tmr_blk,
            fadt->pm_tmr_len
        );

        printf("\tGPE0: block=0x%08" PRIX32 ", length=%d\n", fadt->gpe0_blk, fadt->gpe0_blk_len);
        printf(
            "\tGPE1: base=%d, block=0x%08" PRIX32 ", length=%d\n",
            fadt->gpe1_base,
            fadt->gpe1_blk,
            fadt->gpe1_blk_len
        );

        printf("\tCSTATE Control: %02X\n", fadt->cst_cnt);
        printf("\tWorst Latency: c2=%u c3=%u\n", fadt->p_lvl2_lat, fadt->p_lvl3_lat);
        printf("\tCPU Cache Flush: size=%u, stride=%u\n", fadt->flush_size, fadt->flush_stride);
        printf("\tDuty Cycle Setting: offset=%u, width=%u\n", fadt->duty_offset, fadt->duty_width);
        printf("\tRTC Alarm Offset: day=%u, month=%u\n", fadt->day_alrm, fadt->mon_alrm);
        printf("\tRTC Century Offset: %u\n", fadt->century);

        if (fadt->hdr.revision >= 3) {
            printf("\tIA-PC Boot Architecture Flags: %04X\n", fadt->iapc_boot_arch);
        }
    }

    struct uacpi_table table;
    struct acpi_madt *madt;
    if (!uacpi_table_find_by_signature(ACPI_MADT_SIGNATURE, &table)) {
        madt = table.ptr;
        printf("MADT: 0x%p\n", (void *)madt);

        printf("\tLocal APIC Address: 0x%08" PRIX32 "\n", madt->local_interrupt_controller_address);
        printf("\t8259 PIC Installed: %s\n", (madt->flags & 1) ? "true" : "false");

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
                printf("\tProcessor Local APIC Entry:\n");
                printf("\t\tACPI Processor ID: 0x%02X\n", entry->lapic.uid);
                printf("\t\tLAPIC ID: 0x%02X\n", entry->lapic.id);
                printf("\t\tFlags: 0x%08" PRIX32 "\n", entry->lapic.flags);
                break;
            case ACPI_MADT_ENTRY_TYPE_IOAPIC:
                printf("\tI/O APIC Entry:\n");
                printf("\t\tID: 0x%02X\n", entry->ioapic.id);
                printf("\t\tAddress: 0x%08" PRIX32 "\n", entry->ioapic.address);
                printf(
                    "\t\tGlobal System Interrupt Base: 0x%08" PRIX32 "\n",
                    entry->ioapic.gsi_base
                );
                break;
            case ACPI_MADT_ENTRY_TYPE_INTERRUPT_SOURCE_OVERRIDE:
                printf("\tInterrupt Source Override Entry:\n");
                printf("\t\tBus: 0x%02X\n", entry->interrupt_source_override.bus);
                printf("\t\tIRQ: 0x%02X\n", entry->interrupt_source_override.source);
                printf(
                    "\t\tGlobal System Interrupt: 0x%08" PRIX32 "\n",
                    entry->interrupt_source_override.gsi
                );
                printf("\t\tFlags: 0x%04X\n", entry->interrupt_source_override.flags);
                break;
            case ACPI_MADT_ENTRY_TYPE_NMI_SOURCE:
                printf("\tNMI Source Entry:\n");
                break;
            case ACPI_MADT_ENTRY_TYPE_LAPIC_NMI:
                printf("\tLocal APIC NMI Entry:\n");
                printf("\t\tACPI Processor ID: 0x%02X\n", entry->lapic_nmi.uid);
                printf("\t\tFlags: 0x%04X\n", entry->lapic_nmi.flags);
                printf("\t\tLocal APIC LINT#: 0x%02X\n", entry->lapic_nmi.lint);
                break;
            case ACPI_MADT_ENTRY_TYPE_LAPIC_ADDRESS_OVERRIDE:
                printf("\tLocal APIC Address Override Entry:\n");
                printf("\t\tAddress: 0x%016" PRIX64 "\n", entry->lapic_address_override.address);
                break;
            default:
                break;
            }

            entry = (void *)((uint8_t *)entry + entry->header.length);
        }
    }

    struct acpi_hpet *hpet;
    if (!uacpi_table_find_by_signature(ACPI_HPET_SIGNATURE, &table)) {
        hpet = table.ptr;
        printf("HPET: 0x%p\n", (void *)hpet);

        printf("\tBlock ID: %08" PRIX32 "\n", hpet->block_id);
        printf(
            "\tAddress: asp=%u width=%u offset=%u asz=%u address=%016" PRIX64 "\n",
            hpet->address.address_space_id,
            hpet->address.register_bit_width,
            hpet->address.register_bit_offset,
            hpet->address.access_size,
            hpet->address.address
        );
        printf("\tNumber: %u\n", hpet->number);
        printf("\tMinimum Clock Tick: %u\n", hpet->min_clock_tick);
        printf("\tFlags: %02X\n", hpet->flags);
    }

    return 0;
}

static struct command acpi_command = {
    .name = "acpi",
    .handler = acpi_handler,
    .help_message = "Show ACPI information",
};

static void acpi_command_init(void)
{
    VlShell_RegisterCommand(&acpi_command);
}

REGISTER_SHELL_COMMAND(acpi, acpi_command_init)
