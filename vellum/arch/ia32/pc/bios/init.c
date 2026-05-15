#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vellum/arch/cpufeatures.h>
#include <vellum/arch/interrupt.h>
#include <vellum/arch/intrinsics/io.h>

#include <vellum/plat/bios/bootinfo.h>
#include <vellum/plat/bios/disk.h>
#include <vellum/plat/bios/mem.h>
#include <vellum/plat/bios/video.h>
#include <vellum/plat/gdt.h>
#include <vellum/plat/isr.h>
#include <vellum/plat/panic.h>
#include <vellum/plat/pic.h>

#include <vellum/compiler.h>
#include <vellum/device.h>
#include <vellum/disk.h>
#include <vellum/filesystem.h>
#include <vellum/global_configs.h>
#include <vellum/interface/block.h>
#include <vellum/log.h>
#include <vellum/macros.h>
#include <vellum/mm.h>
#include <vellum/resource.h>
#include <vellum/status.h>
#include <vellum/types.h>

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

static VlStatus init_pma(void)
{
    VlStatus status;
    uint32_t smap_cursor;
    struct smap_entry smap_entry;
    uint64_t smap_base, smap_size;
    uintptr_t base_paddr, limit_paddr, pma_limit_paddr;

    /* calculate available area that covers free memory from 0x100000 to 0xFFFFFFFF */
    smap_cursor = 0;
    base_paddr = 0;
    limit_paddr = 0;
    do {
        status = VlBiosP_QueryMemoryMap(&smap_cursor, &smap_entry, sizeof(smap_entry));
        if (!CHECK_SUCCESS(status)) return status;

        smap_base = (uint64_t)smap_entry.base_addr_high << 32 | smap_entry.base_addr_low;
        smap_size = (uint64_t)smap_entry.length_high << 32 | smap_entry.length_low;
        if (!smap_size) continue;

        if (smap_entry.type != 0x00000001) continue;
        if (smap_base > UINTPTR_MAX) continue;

        uint64_t smap_limit = smap_base + smap_size - 1;
        if (smap_limit > UINTPTR_MAX || smap_limit < smap_base) {
            smap_limit = UINTPTR_MAX;
        }

        if (smap_limit < 0x100000) continue;
        if (smap_base < 0x100000) {
            smap_base = 0x100000;
        }

        if (!base_paddr || smap_base < base_paddr) {
            base_paddr = smap_base;
        }
        if (limit_paddr < smap_limit) {
            limit_paddr = smap_limit;
        }
    } while (smap_cursor);

    if (!base_paddr || limit_paddr < base_paddr) return STATUS_INSUFFICIENT_MEMORY;

    pma_limit_paddr = limit_paddr;

    status = mm_pma_init(base_paddr, limit_paddr);
    if (!CHECK_SUCCESS(status)) return status;

    /* mark smaller reserved area inside the previous area */
    smap_cursor = 0;
    do {
        status = VlBiosP_QueryMemoryMap(&smap_cursor, &smap_entry, sizeof(smap_entry));
        if (!CHECK_SUCCESS(status)) return status;

        smap_base = (uint64_t)smap_entry.base_addr_high << 32 | smap_entry.base_addr_low;
        smap_size = (uint64_t)smap_entry.length_high << 32 | smap_entry.length_low;
        if (!smap_size) continue;

        if (smap_entry.type == 0x00000001) continue;
        if (smap_base > UINTPTR_MAX) continue;

        uint64_t smap_limit = smap_base + smap_size - 1;
        if (smap_limit > UINTPTR_MAX || smap_limit < smap_base) {
            smap_limit = UINTPTR_MAX;
        }

        if (smap_limit < 0x100000) continue;
        if (smap_base > pma_limit_paddr) continue;
        if (smap_base < 0x100000) {
            smap_base = 0x100000;
        }
        if (smap_limit > pma_limit_paddr) {
            smap_limit = pma_limit_paddr;
        }

        base_paddr = smap_base;
        limit_paddr = smap_limit;

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
    VlStatus status;

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

static VlStatus probe_bios_disk(
    struct device_driver *drv, uint8_t drive, int required, int skip_partitions
)
{
    VlStatus status;
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

static VlStatus init_bios_disks(void)
{
    VlStatus status;
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

static VlStatus init_nonpnp_devices(void)
{
    VlStatus status;
    int skip_legacy = 0, skip_rtc = 0;

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

    /* Use BIOS keyboard services while the firmware is still available. */
    {
        LOG_DEBUG("initializing BIOS keyboard...\n");

        struct device *dev;
        struct device_driver *drv;

        status = VlDev_FindDriver("bioskbd", &drv);
        if (!CHECK_SUCCESS(status)) return status;

        status = drv->probe(&dev, drv, NULL, NULL, 0);
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

int config_rtc_century_offset;

VlStatus mount_boot_filesystem(void)
{
    VlStatus status;
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

static VlStatus reload_boot_sector(void)
{
    VlStatus status;
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
    VlStatus status;

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

    VlPicP_RemapInterrupt(0x20, 0x28);
    for (int i = 0x20; i < 0x30; i++) {
        VlIntP_Mask(i);
    }

    VlIntP_AddTrapHandler(0x03, bkpt_handler, NULL);
    VlIntP_AddInterruptHandler(0x20, NULL, tick_isr, NULL);

    LOG_DEBUG("initializing PIT...\n");
    init_timer();

    LOG_DEBUG("running constructors...\n");
    for (int i = 0; &__init_array_start[i] != __init_array_end; i++) {
        __init_array_start[i]();
    }

    VlA_EnableInterrupt();

    LOG_DEBUG("initializing non-PnP devices...\n");
    status = init_nonpnp_devices();
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
    VlPicP_RemapInterrupt(0x08, 0x70);
}
