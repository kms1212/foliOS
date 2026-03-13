#include <vellum/shell.h>

#include <inttypes.h>
#include <stdio.h>

#include <vellum/plat/bios/mem.h>

#include <vellum/mm.h>
#include <vellum/status.h>

static int mem_handler(struct shell_instance *inst, int argc, char **argv)
{
    status_t status;
    size_t total_pages, free_pages;
    uint32_t cursor = 0;
    struct smap_entry entry;

    static const char *region_type[] = {
        "",
        "Free",
        "Reserved",
        "ACPI Reclaimable",
        "ACPI NVS",
        "Bad",
    };

    printf("Memory Map:\n");
    printf("Base             Size             Type\n");
    do {
        VlBiosP_QueryMemoryMap(&cursor, &entry, sizeof(entry));
        printf(
            "%08" PRIX32 "%08" PRIX32 " %08" PRIX32 "%08" PRIX32 " %s\n",
            entry.base_addr_high,
            entry.base_addr_low,
            entry.length_high,
            entry.length_low,
            region_type[entry.type]
        );
    } while (cursor);

    status = mm_pma_get_available_frame_count(&total_pages);
    if (!CHECK_SUCCESS(status)) {
        fprintf(stderr, "%s: unable to query memory usage\n", argv[0]);
        return 1;
    }

    status = mm_pma_get_free_frame_count(&free_pages);
    if (!CHECK_SUCCESS(status)) {
        fprintf(stderr, "%s: unable to query memory usage\n", argv[0]);
        return 1;
    }

    printf(
        "%zu bytes allocatable memory, %zu bytes free (%zu%%)\n",
        total_pages * PAGE_SIZE,
        free_pages * PAGE_SIZE,
        free_pages * 100 / total_pages
    );

    return 0;
}

static struct command mem_command = {
    .name = "mem",
    .handler = mem_handler,
    .help_message = "Print memory usage",
};

static void mem_command_init(void)
{
    VlShell_RegisterCommand(&mem_command);
}

REGISTER_SHELL_COMMAND(mem, mem_command_init)
