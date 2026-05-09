#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vellum/arch/cpufeatures.h>
#include <vellum/arch/idt.h>
#include <vellum/arch/intrinsics/rdtsc.h>
#include <vellum/arch/io.h>

#include <vellum/plat/apm.h>
#include <vellum/plat/bios/bootinfo.h>
#include <vellum/plat/bios/disk.h>
#include <vellum/plat/bios/keyboard.h>
#include <vellum/plat/bios/mem.h>
#include <vellum/plat/bios/misc.h>
#include <vellum/plat/bios/video.h>
#include <vellum/plat/gdt.h>
#include <vellum/plat/instruction.h>
#include <vellum/plat/isr.h>
#include <vellum/plat/pci/cfgspace.h>
#include <vellum/plat/pic.h>

#include <vellum/compiler.h>
#include <vellum/debug.h>
#include <vellum/device.h>
#include <vellum/filesystem.h>
#include <vellum/global_configs.h>
#include <vellum/interface/block.h>
#include <vellum/interface/char.h>
#include <vellum/interface/console.h>
#include <vellum/interface/framebuffer.h>
#include <vellum/log.h>
#include <vellum/macros.h>
#include <vellum/mm.h>
#include <vellum/panic.h>
#include <vellum/status.h>

#include <uacpi/event.h>
#include <uacpi/tables.h>
#include <uacpi/uacpi.h>

#define MODULE_NAME "init"

extern void main(void);

extern void (*__init_array_start[])(void);
extern void (*__init_array_end[])(void);

static ssize_t early_stderr_write(void *cookie, const char *buf, size_t count)
{
    for (size_t i = 0; buf[i] && i < count; i++) {
        if (buf[i] == '\n') {
            VlBiosP_WriteVideoTty('\r');
        }
        VlBiosP_WriteVideoTty(buf[i]);
    }

    return (ssize_t)count;
}

static const struct cookie_io_functions early_stderr_io = {
    .write = early_stderr_write,
};

static ssize_t early_stddbg_write(void *cookie, const char *buf, size_t count)
{
    for (size_t i = 0; buf[i] && i < count; i++) {
        VlA_Out8(0x00E9, buf[i]);
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
        VlBiosP_QueryMemoryMap(&smap_cursor, &smap_entry, sizeof(smap_entry));
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
        VlBiosP_QueryMemoryMap(&smap_cursor, &smap_entry, sizeof(smap_entry));
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

static uint8_t get_bios_fixed_disk_count(void)
{
    const volatile uint8_t *bda_count = (const volatile uint8_t *)0x475;
    uint8_t int13_count = 0;
    uint8_t count = *bda_count;
    status_t status;

    status = VlBiosP_GetDiskParams(0x80, &int13_count, NULL, NULL, NULL);
    if (CHECK_SUCCESS(status) && count < int13_count) {
        count = int13_count;
    }

    return count;
}

static uint8_t get_bios_removable_disk_count(void)
{
    const volatile uint16_t *equipment = (const volatile uint16_t *)0x410;
    uint16_t flags = *equipment;
    uint8_t count = 0;

    if (flags & 0x0001) {
        count = (uint8_t)(((flags >> 6) & 0x3) + 1);
    }

    if (!(_pc_boot_drive & 0x80) && count <= _pc_boot_drive) {
        count = (uint8_t)(_pc_boot_drive + 1);
    }

    return count;
}

static status_t probe_bios_disk(
    struct device_driver *drv, uint8_t drive, int required, int skip_partitions
)
{
    status_t status;
    struct device *dev = NULL;

    struct resource res[] = {
        {
            .type = RT_BUS,
            .base = drive,
            .limit = drive,
            .flags = skip_partitions ? BIOS_DISK_RESOURCE_SKIP_PARTITIONS : 0,
        },
    };

    status = drv->probe(&dev, drv, NULL, res, ARRAY_SIZE(res));
    if (!CHECK_SUCCESS(status)) {
        if (required) return status;

        LOG_TRACE("BIOS disk %02X unavailable: %08lX\n", drive, status);
    }

    return STATUS_SUCCESS;
}

static status_t init_bios_disks(void)
{
    status_t status;
    struct device_driver *drv;
    uint8_t fixed_count;
    uint8_t removable_count;

    status = VlDev_FindDriver("biosdisk", &drv);
    if (!CHECK_SUCCESS(status)) return status;

    status = probe_bios_disk(drv, _pc_boot_drive, 1, 0);
    if (!CHECK_SUCCESS(status)) return status;

    fixed_count = get_bios_fixed_disk_count();
    for (uint8_t i = 0; i < fixed_count; i++) {
        uint8_t drive = (uint8_t)(0x80 + i);

        if (drive == _pc_boot_drive) continue;

        status = probe_bios_disk(drv, drive, 0, 0);
        if (!CHECK_SUCCESS(status)) return status;
    }

    removable_count = get_bios_removable_disk_count();
    for (uint8_t i = 0; i < removable_count; i++) {
        uint8_t drive = i;

        if (drive == _pc_boot_drive) continue;

        status = probe_bios_disk(drv, drive, 0, 1);
        if (!CHECK_SUCCESS(status)) return status;
    }

    for (uint8_t drive = 0xE0; drive < 0xF0; drive++) {
        if (drive == _pc_boot_drive) continue;

        status = probe_bios_disk(drv, drive, 0, 1);
        if (!CHECK_SUCCESS(status)) return status;
    }

    return STATUS_SUCCESS;
}

static status_t init_nonpnp_devices(int has_acpi)
{
    status_t status;
    uacpi_status uacpi_status;
    int skip_legacy = 0, skip_8042 = 0, skip_rtc = 0;
    struct acpi_fadt *fadt;

    if (has_acpi) {
        LOG_DEBUG("retrieving ACPI FADT...\n");

        uacpi_status = uacpi_table_fadt(&fadt);
        if (uacpi_unlikely_error(uacpi_status)) {
            VlP_Panic(uacpi_status, "Could not find FADT (has_acpi=%d)", has_acpi);
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
        LOG_DEBUG("initializing port 0xE9...\n");

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

        status = VlDev_FindDriver("debugout", &drv);
        if (!CHECK_SUCCESS(status)) return status;

        status = drv->probe(&dev, drv, NULL, res, ARRAY_SIZE(res));
        if (!CHECK_SUCCESS(status)) return status;

        if (freopendevice("dbg0", stddbg)) return STATUS_UNKNOWN_ERROR;
    }

#endif

    /* find Non-PnP ISA Components */
    if (!skip_8042) {
        LOG_DEBUG("initializing i8042...\n");

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

        status = VlDev_FindDriver("i8042", &drv);
        if (!CHECK_SUCCESS(status)) return status;

        status = drv->probe(&dev, drv, NULL, res, ARRAY_SIZE(res));
        if (!CHECK_SUCCESS(status)) return status;
    }

    if (!skip_rtc) {
        LOG_DEBUG("initializing RTC...\n");

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

        status = VlDev_FindDriver("rtc_isa", &drv);
        if (!CHECK_SUCCESS(status)) return status;

        status = drv->probe(&dev, drv, NULL, res, ARRAY_SIZE(res));
        if (!CHECK_SUCCESS(status)) return status;
    }

    if (!skip_legacy) {
        for (int i = 0; i < 4; i++) {
            LOG_DEBUG("initializing UART #%d...\n", i);

            uint16_t *io_base_list = (uint16_t *)0x400;

            // a workaround to make the compiler shut up in release build
            __asm__ volatile("" : "+g"(io_base_list));

            uint16_t io_base = io_base_list[i];
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

            status = VlDev_FindDriver("uart_isa", &drv);
            if (!CHECK_SUCCESS(status)) return status;

            status = drv->probe(&dev, drv, NULL, res, ARRAY_SIZE(res));
            if (!CHECK_SUCCESS(status)) return status;
        }

        for (int i = 0; i < 3; i++) {
            LOG_DEBUG("initializing IEEE1284 #%d...\n", i);

            uint16_t *io_base_list = (uint16_t *)0x408;

            // a workaround to make the compiler shut up in release build
            __asm__ volatile("" : "+g"(io_base_list));

            uint16_t io_base = io_base_list[i];
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

            status = VlDev_FindDriver("ieee1284_isa", &drv);
            if (!CHECK_SUCCESS(status)) return status;

            status = drv->probe(&dev, drv, NULL, res, ARRAY_SIZE(res));
            if (!CHECK_SUCCESS(status)) return status;
        }
    }

    {
        LOG_DEBUG("initializing BIOS disks...\n");
        status = init_bios_disks();
        if (!CHECK_SUCCESS(status)) return status;
    }

    {
        LOG_DEBUG("initializing VGA...\n");

        struct device *dev;
        struct device_driver *drv;

        status = VlDev_FindDriver("vga", &drv);
        if (!CHECK_SUCCESS(status)) return status;

        status = drv->probe(&dev, drv, NULL, NULL, 0);
        if (!CHECK_SUCCESS(status)) return status;
    }

    return 0;
}

#define MAKE_ACPI_STATUS(uacpi_status)                                                             \
    ((uacpi_status) ? (0x80010000 | (uacpi_status)) : STATUS_SUCCESS)

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

status_t mount_boot_filesystem(void)
{
    status_t status;
    struct device *bootdisk;
    const struct block_interface *blki;
    uint8_t sect0[512];
    size_t sect_size;

    bootdisk = VlDev_GetFirst();

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

    status = VlFs_MountAuto(bootdisk, "boot");
    if (!CHECK_SUCCESS(status)) return status;

    return STATUS_SUCCESS;
}

static volatile uint64_t global_tick = 0;

uint64_t get_global_tick(void)
{
    return global_tick;
}

static void tick_isr(void *data, struct VlA_InterruptFrame *frame, struct trap_regs *regs, int num)
{
    global_tick++;
}

static void bkpt_handler(struct VlA_InterruptFrame *frame, struct trap_regs *regs, int num)
{
    fprintf(stderr, "Breakpoint at %04" PRIX16 ":%08" PRIX32 "\n", frame->cs, frame->eip);

    fprintf(
        stderr,
        "EAX=%08" PRIX32 " EBX=%08" PRIX32 " ECX=%08" PRIX32 " EDX=%08" PRIX32 "\n",
        regs->eax,
        regs->ebx,
        regs->ecx,
        regs->edx
    );
    fprintf(
        stderr,
        "ESI=%08" PRIX32 " EDI=%08" PRIX32 " EBP=%08" PRIX32 " ESP=%08" PRIX32 "\n",
        regs->esi,
        regs->edi,
        regs->ebp,
        regs->esp
    );
    fprintf(
        stderr,
        "CS=%04" PRIX16 " DS=%04" PRIX16 " ES=%04" PRIX16 " FS=%04" PRIX16 " GS=%04" PRIX16 "\n",
        frame->cs,
        regs->ds,
        regs->es,
        regs->fs,
        regs->gs
    );
    fprintf(stderr, "EFLAGS=%08" PRIX32 "\n", frame->eflags);

    uint32_t bp = regs->ebp;
    for (int i = 0; bp; i++) {
        fprintf(
            stderr,
            "Frame #%d: %08" PRIX32 " %04" PRIX16 ":%08" PRIX32 "\n",
            i,
            bp,
            frame->cs,
            ((uint32_t *)bp)[1]
        );
        bp = ((uint32_t *)bp)[0];
    }
}

static void init_timer(void)
{
    static const uint16_t pit_value = 1193182 / 20;

    VlA_Out8(0x0043, 0x34);
    VlA_Out8(0x0040, pit_value & 0xFF);
    VlA_Out8(0x0040, (pit_value >> 8) & 0xFF);

    VlIntP_Unmask(0x20);
}

static status_t reload_boot_sector(void)
{
    status_t status;
    uint16_t edd_features;

    status = VlBiosP_CheckDiskExtension(_pc_boot_drive, NULL, &edd_features);
    if (CHECK_SUCCESS(status) && (edd_features & EXT_FEATURE_PACKET)) {
        return VlBiosP_ReadDiskExtended(_pc_boot_drive, _pc_boot_part_base, 1, _pc_boot_sector);
    }

    struct chs bootdisk_geom;
    status = VlBiosP_GetDiskParams(_pc_boot_drive, NULL, NULL, &bootdisk_geom, NULL);
    if (!CHECK_SUCCESS(status)) return status;

    struct chs chs = VlDisk_LbaToChs(_pc_boot_part_base, bootdisk_geom);
    return VlBiosP_ReadDisk(_pc_boot_drive, chs, 1, _pc_boot_sector, NULL);
}

__noreturn void _pc_init(void)
{
    status_t status;
    int has_acpi = 0;

    freopencookie(NULL, "w", early_stderr_io, stderr);
    freopencookie(NULL, "w", early_stddbg_io, stddbg);

    VlLog_SetLevel(LL_DEBUG);

    LOG_DEBUG("Starting Vellum...\n");

    LOG_DEBUG("Checking CPU Features...\n");
    status = VlA_CheckCpuFeatures();
    if (!CHECK_SUCCESS(status)) {
        VlP_Panic(status, "failed to check CPU features");
    }

    LOG_DEBUG("initializing ISRs...\n");
    VlIntP_Init();

    LOG_DEBUG("initializing GDT...\n");
    _pc_gdt_init();

    LOG_DEBUG("initializing physical memory allocator...\n");
    status = init_pma();
    if (!CHECK_SUCCESS(status)) {
        VlP_Panic(status, "failed to initialize physical memory allocator");
    }

    LOG_DEBUG("initializing memory management...\n");
    status = mm_init();
    if (!CHECK_SUCCESS(status)) {
        VlP_Panic(status, "failed to initialize memory management");
    }

    LOG_DEBUG("reloading VBR sector...\n");
    status = reload_boot_sector();
    if (!CHECK_SUCCESS(status)) {
        VlP_Panic(
            status,
            "could not reload VBR sector %02X:%08" PRIX32,
            _pc_boot_drive,
            _pc_boot_part_base
        );
    }

    _pc_pic_remap_int(0x20, 0x28);

    VlIntP_AddTrapHandler(0x03, bkpt_handler, NULL);
    VlIntP_AddInterruptHandler(0x20, NULL, tick_isr, NULL);

    LOG_DEBUG("initializing PIT...\n");
    init_timer();

    LOG_DEBUG("early-initializing ACPI...\n");
    status = acpi_early_init();
    has_acpi = CHECK_SUCCESS(status);
    if (!has_acpi) {
        LOG_DEBUG("ACPI is not available\n");
    }

    LOG_DEBUG("running constructors...\n");
    for (int i = 0; &__init_array_start[i] != __init_array_end; i++) {
        __init_array_start[i]();
    }

    VlA_EnableInterrupt();

    LOG_DEBUG("initializing non-PnP devices...\n");
    status = init_nonpnp_devices(has_acpi);
    if (!CHECK_SUCCESS(status)) {
        fprintf(stderr, "init_nonpnp_devices() failed: 0x%08lX\n", status);
        VlP_Panic(status, "failed to initialize essential non-PnP devices");
    }

    LOG_DEBUG("mounting boot filesystem...\n");
    status = mount_boot_filesystem();
    if (!CHECK_SUCCESS(status)) {
        LOG_WARN("failed to mount Vellum partition");
    }

    LOG_DEBUG("starting main...\n");
    main();

    VlP_Panic(STATUS_UNKNOWN_ERROR, "Kernel returned");
}

void _pc_cleanup(void)
{
    _pc_pic_remap_int(0x08, 0x70);
}
