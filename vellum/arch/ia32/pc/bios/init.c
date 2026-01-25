#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <vellum/asm/bios/bootinfo.h>
#include <vellum/asm/bios/disk.h>
#include <vellum/asm/bios/video.h>
#include <vellum/asm/bios/keyboard.h>
#include <vellum/asm/bios/mem.h>
#include <vellum/asm/bios/misc.h>
#include <vellum/asm/pci/cfgspace.h>
#include <vellum/asm/io.h>
#include <vellum/asm/idt.h>
#include <vellum/asm/isr.h>
#include <vellum/asm/pic.h>
#include <vellum/asm/pc_gdt.h>
#include <vellum/asm/apm.h>
#include <vellum/asm/instruction.h>
#include <vellum/asm/intrinsics/rdtsc.h>

#include <vellum/compiler.h>
#include <vellum/status.h>
#include <vellum/panic.h>
#include <vellum/macros.h>
#include <vellum/debug.h>
#include <vellum/log.h>
#include <vellum/mm.h>
#include <vellum/device.h>
#include <vellum/global_configs.h>
#include <vellum/filesystem.h>
#include <vellum/interface/char.h>
#include <vellum/interface/block.h>
#include <vellum/interface/console.h>
#include <vellum/interface/framebuffer.h>

#include <uacpi/uacpi.h>
#include <uacpi/event.h>
#include <uacpi/tables.h>

#define MODULE_NAME "init"

extern void main(void);

extern void (*_init_array_start_)(void);
extern void (*_init_array_end_)(void);

static ssize_t early_stderr_write(void *cookie, const char *buf, size_t count)
{
    for (size_t i = 0; buf[i] && i < count; i++) {
        if (buf[i] == '\n') {
            _pc_bios_video_write_tty('\r');
        }
        _pc_bios_video_write_tty(buf[i]);
    }

    return (ssize_t)count;
}

static const struct cookie_io_functions early_stderr_io = {
    .write = early_stderr_write,
};

static ssize_t early_stddbg_write(void *cookie, const char *buf, size_t count)
{
    for (size_t i = 0; buf[i] && i < count; i++) {
        io_out8(0x00E9, buf[i]);
    }

    return (ssize_t)count;
}

static const struct cookie_io_functions early_stddbg_io = {
    .write = early_stddbg_write,
};

static status_t init_pma(void)
{
    status_t status;
    uint32_t smap_cursor;
    struct smap_entry smap_entry;
    uint64_t smap_base, smap_size;
    uintptr_t base_paddr, limit_paddr;

    /* calculate available area that covers free memory from 0x100000 to 0xFFFFFFFF */
    smap_cursor = 0;
    base_paddr = 0;
    limit_paddr = 0;
    do {
        _pc_bios_mem_query_map(&smap_cursor, &smap_entry, sizeof(smap_entry));
        smap_base = (uint64_t)smap_entry.base_addr_high << 32 | smap_entry.base_addr_low;
        smap_size = (uint64_t)smap_entry.length_high << 32 | smap_entry.length_low;

        if (smap_base < 0x100000) continue;
        if (smap_entry.type != 0x00000001) continue;

        if (!base_paddr) {
            base_paddr = smap_base;
        }
        if (limit_paddr < smap_base + smap_size - 1) {
            limit_paddr = smap_base + smap_size - 1;
        }
    } while (smap_cursor);

    status = mm_pma_init(base_paddr, limit_paddr);
    if (!CHECK_SUCCESS(status)) return status;

    /* mark smaller reserved area inside the previous area */
    smap_cursor = 0;
    do {
        _pc_bios_mem_query_map(&smap_cursor, &smap_entry, sizeof(smap_entry));
        smap_base = (uint64_t)smap_entry.base_addr_high << 32 | smap_entry.base_addr_low;
        smap_size = (uint64_t)smap_entry.length_high << 32 | smap_entry.length_low;

        if (smap_base + smap_size <= 0x100000) continue;
        if (smap_base > limit_paddr) continue;
        if (smap_entry.type == 0x00000001) continue;

        if (smap_base < 0x100000) {
            smap_size -= 0x100000 - smap_base;
            smap_base = 0x100000;
        }

        base_paddr = smap_base;
        limit_paddr = smap_base + smap_size - 1;

        status = mm_pma_mark_reserved(base_paddr, limit_paddr);
        if (!CHECK_SUCCESS(status)) return status;
    } while (smap_cursor);
    
    return STATUS_SUCCESS;
}

static status_t init_nonpnp_devices(int has_acpi)
{
    status_t status;
    uacpi_status uacpi_status;
    int skip_legacy = 0, skip_8042 = 0, skip_rtc = 0;
    struct acpi_fadt *fadt;
    
    if (has_acpi) {
        uacpi_status = uacpi_table_fadt(&fadt);
        if (uacpi_unlikely_error(uacpi_status)) {
            panic(uacpi_status, "Could not find FADT (has_acpi=%d)", has_acpi);
        }

        if (fadt->hdr.revision >= 3) {
            if (!(fadt->iapc_boot_arch & ACPI_IA_PC_LEGACY_DEVS)) {
                skip_legacy = 1;
            }
    
            if (!(fadt->iapc_boot_arch & ACPI_IA_PC_8042)) {
                skip_8042 = 1;
            }
    
            if (fadt->iapc_boot_arch & ACPI_IA_PC_NO_CMOS_RTC) {
                skip_rtc = 1;
            }
        }
    }

#ifndef NDEBUG
    {
        struct device *dev;
        struct device_driver *drv;

        struct resource res[] = {
            {
                .type = RT_IOPORT,
                .base = 0x00E9,
                .limit = 0x00E9,
                .flags = 0,
            },
        };

        status = device_driver_find("debugout", &drv);
        if (!CHECK_SUCCESS(status)) return status;

        status = drv->probe(&dev, drv, NULL, res, ARRAY_SIZE(res));
        if (!CHECK_SUCCESS(status)) return status;

        if (freopendevice("dbg0", stddbg)) return STATUS_UNKNOWN_ERROR;
    }

#endif

    /* find Non-PnP ISA Components */
    if (!skip_8042) {
        struct device *dev;
        struct device_driver *drv;

        struct resource res[] = {
            {
                .type = RT_IOPORT,
                .base = 0x0060,
                .limit = 0x0060,
                .flags = 0,
            },
            {
                .type = RT_IOPORT,
                .base = 0x0064,
                .limit = 0x0064,
                .flags = 0,
            },
            {
                .type = RT_IRQ,
                .base = 0x21,
                .limit = 0x21,
                .flags = 0,
            },
            {
                .type = RT_IRQ,
                .base = 0x2C,
                .limit = 0x2C,
                .flags = 0,
            },
        };

        status = device_driver_find("i8042", &drv);
        if (!CHECK_SUCCESS(status)) return status;

        status = drv->probe(&dev, drv, NULL, res, ARRAY_SIZE(res));
        if (!CHECK_SUCCESS(status)) return status;
    }

    if (!skip_rtc) {
        struct device *dev;
        struct device_driver *drv;

        struct resource res[] = {
            {
                .type = RT_IOPORT,
                .base = 0x0070,
                .limit = 0x0071,
                .flags = 0,
            },
            {
                .type = RT_IRQ,
                .base = 0x28,
                .limit = 0x28,
                .flags = 0,
            },
        };

        status = device_driver_find("rtc_isa", &drv);
        if (!CHECK_SUCCESS(status)) return status;

        status = drv->probe(&dev, drv, NULL, res, ARRAY_SIZE(res));
        if (!CHECK_SUCCESS(status)) return status;
    }

    if (!skip_legacy) {
        for (int i = 0; i < 4; i++) {
            uint16_t io_base = ((uint16_t *)0x400)[i];
            uint8_t irq_num = (i & 1) ? 0x23 : 0x24;

            if (!io_base) continue;

            struct device *dev;
            struct device_driver *drv;
    
            struct resource res[] = {
                {
                    .type = RT_IOPORT,
                    .base = io_base,
                    .limit = io_base + 8,
                    .flags = 0,
                },
                {
                    .type = RT_IRQ,
                    .base = irq_num,
                    .limit = irq_num,
                    .flags = 0,
                },
            };

            status = device_driver_find("uart_isa", &drv);
            if (!CHECK_SUCCESS(status)) return status;
    
            status = drv->probe(&dev, drv, NULL, res, ARRAY_SIZE(res));
            if (!CHECK_SUCCESS(status)) return status;
        }

        for (int i = 0; i < 3; i++) {
            uint16_t io_base = ((uint16_t *)0x408)[i];
            uint8_t irq_num = 0x27 - i;

            if (!io_base) continue;

            struct device *dev;
            struct device_driver *drv;
    
            struct resource res[] = {
                {
                    .type = RT_IOPORT,
                    .base = io_base,
                    .limit = io_base + 2,
                    .flags = 0,
                },
                {
                    .type = RT_IRQ,
                    .base = irq_num,
                    .limit = irq_num,
                    .flags = 0, 
                },
            };

            status = device_driver_find("ieee1284_isa", &drv);
            if (!CHECK_SUCCESS(status)) return status;
    
            status = drv->probe(&dev, drv, NULL, res, ARRAY_SIZE(res));
            if (!CHECK_SUCCESS(status)) return status;
        }

        {
            struct device *dev;
            struct device_driver *drv;
    
            struct resource res[] = {
                {
                    .type = RT_IOPORT,
                    .base = 0x03F0,
                    .limit = 0x03F7,
                    .flags = 0,
                },
                {
                    .type = RT_IRQ,
                    .base = 0x26,
                    .limit = 0x26,
                    .flags = 0,
                },
                {
                    .type = RT_DMA,
                    .base = 0,
                    .limit = 0,
                    .flags = 0,
                },
            };

            status = device_driver_find("fdc_isa", &drv);
            if (!CHECK_SUCCESS(status)) return status;
    
            status = drv->probe(&dev, drv, NULL, res, ARRAY_SIZE(res));
            if (!CHECK_SUCCESS(status)) return status;
        }

        {
            struct device *dev;
            struct device_driver *drv;
    
            struct resource res[] = {
                {
                    .type = RT_IOPORT,
                    .base = 0x01F0,
                    .limit = 0x01F7,
                    .flags = 0,
                },
                {
                    .type = RT_IOPORT,
                    .base = 0x03F6,
                    .limit = 0x03F7,
                    .flags = 0,
                },
                {
                    .type = RT_IRQ,
                    .base = 0x2E,
                    .limit = 0x2E,
                    .flags = 0,
                },
            };

            status = device_driver_find("ide_isa", &drv);
            if (!CHECK_SUCCESS(status)) return status;
    
            /* status = */ drv->probe(&dev, drv, NULL, res, ARRAY_SIZE(res));
            // if (!CHECK_SUCCESS(status)) return status;
        }

        {
            struct device *dev;
            struct device_driver *drv;
    
            struct resource res[] = {
                {
                    .type = RT_IOPORT,
                    .base = 0x0170,
                    .limit = 0x0177,
                    .flags = 0,
                },
                {
                    .type = RT_IOPORT,
                    .base = 0x0376,
                    .limit = 0x0377,
                    .flags = 0,
                },
                {
                    .type = RT_IRQ,
                    .base = 0x2F,
                    .limit = 0x2F,
                    .flags = 0,
                },
            };

            status = device_driver_find("ide_isa", &drv);
            if (!CHECK_SUCCESS(status)) return status;
    
            /* status = */ drv->probe(&dev, drv, NULL, res, ARRAY_SIZE(res));
            // if (!CHECK_SUCCESS(status)) return status;
        }
    }
    
    {
        struct device *dev;
        struct device_driver *drv;

        status = device_driver_find("vga", &drv);
        if (!CHECK_SUCCESS(status)) return status;

        status = drv->probe(&dev, drv, NULL, NULL, 0);
        if (!CHECK_SUCCESS(status)) return status;
    }

    return 0;
}

#define MAKE_ACPI_STATUS(uacpi_status) ((uacpi_status) ? (0x80010000 | (uacpi_status)) : STATUS_SUCCESS)

int config_rtc_century_offset;

static status_t acpi_early_init(void)
{
    uacpi_status uacpi_status;
    static uint8_t acpi_buf[PAGE_SIZE];

    uacpi_status = uacpi_setup_early_table_access(acpi_buf, sizeof(acpi_buf));
    if (uacpi_unlikely_error(uacpi_status)) {
        LOG_DEBUG("uACPI initialization failed: %s\n", uacpi_status_to_string(uacpi_status));
        return MAKE_ACPI_STATUS(uacpi_status);
    }

    struct acpi_fadt *fadt;
    uacpi_status = uacpi_table_fadt(&fadt);
    if (uacpi_unlikely_error(uacpi_status)) {
        return MAKE_ACPI_STATUS(uacpi_status);
    }

    config_rtc_century_offset = fadt->century;

    return STATUS_SUCCESS;
}

static status_t acpi_init(void)
{
    uacpi_status uacpi_status;

    uacpi_status = uacpi_initialize(0);
    if (uacpi_unlikely_error(uacpi_status)) {
        LOG_DEBUG("uacpi_initialize() failed: %s\n", uacpi_status_to_string(uacpi_status));
        return MAKE_ACPI_STATUS(uacpi_status);
    }

    uacpi_status = uacpi_namespace_load();
    if (uacpi_unlikely_error(uacpi_status)) {
        LOG_DEBUG("uacpi_namespace_load() failed: %s\n", uacpi_status_to_string(uacpi_status));
        return MAKE_ACPI_STATUS(uacpi_status);
    }

    uacpi_status = uacpi_namespace_initialize();
    if (uacpi_unlikely_error(uacpi_status)) {
        LOG_DEBUG("uacpi_namespace_initialize() failed: %s\n", uacpi_status_to_string(uacpi_status));
        return MAKE_ACPI_STATUS(uacpi_status);
    }

    uacpi_status = uacpi_finalize_gpe_initialization();
    if (uacpi_unlikely_error(uacpi_status)) {
        LOG_DEBUG("uacpi_finalize_gpe_initialization() failed: %s\n", uacpi_status_to_string(uacpi_status));
        return MAKE_ACPI_STATUS(uacpi_status);
    }

    return STATUS_SUCCESS;
}

status_t mount_boot_filesystem(void)
{
    status_t status;
    struct device *bootdisk;
    const struct block_interface *blki;
    uint8_t sect0[512];
    size_t sect_size;

    bootdisk = device_get_first_dev();

    for (; bootdisk; bootdisk = bootdisk->next) {
        status = bootdisk->driver->get_interface(bootdisk, "block", (const void **)&blki);
        if (!CHECK_SUCCESS(status)) continue;

        status = blki->get_block_size(bootdisk, &sect_size);
        if (!CHECK_SUCCESS(status) || sect_size > 512) continue;

        status = blki->read(bootdisk, 0, sect0, 1, NULL);
        if (!CHECK_SUCCESS(status)) continue;

        if (memcmp(_pc_boot_sector, sect0, sizeof(sect0)) == 0) break;
    }

    if (!bootdisk) return STATUS_BOOT_DEVICE_INACCESSIBLE;

    LOG_DEBUG("boot filesystem found from device \"%s\"\n", bootdisk->name);

    status = filesystem_auto_mount(bootdisk, "boot");
    if (!CHECK_SUCCESS(status)) return status;

    return STATUS_SUCCESS;
}

static volatile uint64_t global_tick = 0;

uint64_t get_global_tick(void)
{
    return global_tick;
}

static void pit_isr(void *data, struct interrupt_frame *frame, struct trap_regs *regs, int num)
{
    global_tick++;
}

static void bkpt_handler(struct interrupt_frame *frame, struct trap_regs *regs, int num)
{
    fprintf(stderr, "Breakpoint at %04X:%08lX\n", frame->cs, frame->eip);

    fprintf(stderr, "EAX=%08lX EBX=%08lX ECX=%08lX EDX=%08lX\n", regs->eax, regs->ebx, regs->ecx, regs->edx);
    fprintf(stderr, "ESI=%08lX EDI=%08lX EBP=%08lX ESP=%08lX\n", regs->esi, regs->edi, regs->ebp, regs->esp);
    fprintf(stderr, "CS=%04X DS=%04X ES=%04X FS=%04X GS=%04X\n", frame->cs, regs->ds, regs->es, regs->fs, regs->gs);
    fprintf(stderr, "EFLAGS=%08lX\n", frame->eflags);

    uint32_t bp = regs->ebp;
    for (int i = 0; bp; i++) {
        fprintf(stderr, "Frame #%d: %08lX %04X:%08lX\n", i, bp, frame->cs, ((uint32_t *)bp)[1]);
        bp = ((uint32_t *)bp)[0];
    }
}

static void init_pit(void)
{
    static const uint16_t pit_value = 1193182 / 20;
    
    io_out8(0x0043, 0x34);
    io_out8(0x0040, pit_value & 0xFF);
    io_out8(0x0040, (pit_value >> 8) & 0xFF);

    _pc_isr_unmask_interrupt(0x20);
}

int _pc_invlpg_undefined;
int _pc_rdtsc_undefined;

static void invlpg_test(void)
{
    __asm__ volatile ("invlpg (%0)" : : "r"(0));
}

static void rdtsc_test(void)
{
    uint32_t low, high;
    __asm__ volatile ("rdtsc" : "=a"(low), "=d"(high));
}

__noreturn
void _pc_init(void)
{
    status_t status;
    int has_acpi = 0, has_apm = 0;
    struct chs bootdisk_geom;

    freopencookie(NULL, "w", early_stderr_io, stderr);
    freopencookie(NULL, "w", early_stddbg_io, stddbg);

    LOG_DEBUG("Starting Vellum...\n");

    LOG_DEBUG("testing whether invlpg available...\n");
    status = _pc_instruction_test(invlpg_test, 3, &_pc_invlpg_undefined);
    if (!CHECK_SUCCESS(status)) {
        panic(status, "failed to test instruction invlpg");
    }

    LOG_DEBUG("testing whether rdtsc available...\n");
    status = _pc_instruction_test(rdtsc_test, 2, &_pc_rdtsc_undefined);
    if (!CHECK_SUCCESS(status)) {
        panic(status, "failed to test instruction rdtsc");
    }

    LOG_DEBUG("initializing ISRs...\n");
    _pc_isr_init();

    LOG_DEBUG("initializing GDT...\n");
    _pc_gdt_init();

    LOG_DEBUG("initializing physical memory allocator...\n");
    status = init_pma();
    if (!CHECK_SUCCESS(status)) {
        panic(status, "failed to initialize physical memory allocator");
    }

    LOG_DEBUG("initializing memory management...\n");
    status = mm_init();
    if (!CHECK_SUCCESS(status)) {
        panic(status, "failed to initialize memory management");
    }

    LOG_DEBUG("reloading VBR sector...\n");
    _pc_bios_disk_get_params(_pc_boot_drive, NULL, NULL, &bootdisk_geom, NULL);
    struct chs chs = disk_lba_to_chs(_pc_boot_part_base, bootdisk_geom);
    status = _pc_bios_disk_read(_pc_boot_drive, chs, 1, _pc_boot_sector, NULL);
    if (!CHECK_SUCCESS(status)) {
        panic(status, "could not reload VBR sector %02X:(%d, %d, %d)", _pc_boot_drive, chs.cylinder, chs.head, chs.sector);
    }

    _pc_pic_remap_int(0x20, 0x28);

    _pc_isr_add_trap_handler(0x03, bkpt_handler, NULL);
    _pc_isr_add_interrupt_handler(0x20, NULL, pit_isr, NULL);

    LOG_DEBUG("initializing PIT...\n");
    init_pit();

    LOG_DEBUG("early-initializing ACPI...\n");
    status = acpi_early_init();
    has_acpi = CHECK_SUCCESS(status);
    if (!has_acpi) {
        LOG_DEBUG("ACPI is not available\n");
    }

    LOG_DEBUG("running constructors...\n");
    for (int i = 0; &(&_init_array_start_)[i] != &_init_array_end_; i++) {
        (&_init_array_start_)[i]();
    }

    interrupt_enable();

    LOG_DEBUG("initializing non-PnP devices...\n");
    status = init_nonpnp_devices(has_acpi);
    if (!CHECK_SUCCESS(status)) {
        fprintf(stderr, "init_nonpnp_devices() failed: 0x%08X\n", status);
        panic(status, "failed to initialize essential non-PnP devices");
    }

    if (has_acpi) {
        LOG_DEBUG("initializing ACPI...\n");
        status = acpi_init();
        if (!CHECK_SUCCESS(status)) {
            fprintf(stderr, "acpi_init() failed: 0x%08X\n", status);
            panic(status, "failed to initialize ACPI");
        }
    } else {
        LOG_DEBUG("initializing APM...\n");
        status = _pc_apm_init();
        has_apm = CHECK_SUCCESS(status);
        if (!has_apm) {
            LOG_DEBUG("system power management disabled\n");
        }
    }

    // /* Disable BIOS USB emulation */
    // _bus_pci_cfg_write16(0, 0, 0, 0xC0, 0x2000);
    // _bus_pci_cfg_write16(
    //     uhci_device->bus,
    //     uhci_device->device,
    //     uhci_device->function,
    //     PCI_CFGSPACE_COMMAND,
    //     PCI_COMMAND_BUS_MASTER,
    // );

    // /* List PCI Devices */
    // int pci_count = pci_host_scan(pci_devices, ARRAY_SIZE(pci_devices));

    // /* PC Speaker */
    // io_out8(0x43, 0xb6);
    // io_out8(0x42, (uint8_t)(pit_value / 10));
    // io_out8(0x42, (uint8_t)((pit_value / 10) >> 8));

    // for (int i = 0; i < 4; i++) {
    //     uint64_t tick_start = global_tick;
    //     uint8_t tmp = io_in8(0x61);
    //     io_out8(0x61, tmp | 3);
    //     while (global_tick - tick_start < 50) {}
    //     tick_start = global_tick;
    //     io_out8(0x61, tmp & 0xFC);
    //     while (global_tick - tick_start < 50) {}
    // }

    LOG_DEBUG("mounting boot filesystem...\n");
    status = mount_boot_filesystem();
    if (!CHECK_SUCCESS(status)) {
        LOG_WARN("failed to mount Vellum partition");
    }

    LOG_DEBUG("starting main...\n");
    main();

    panic(STATUS_UNKNOWN_ERROR, "Kernel returned");
}

void _pc_cleanup(void)
{
    _pc_pic_remap_int(0x08, 0x70);
}
