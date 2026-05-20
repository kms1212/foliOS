#include "load_folios.h"

#include <inttypes.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vellum/elf.h>
#include <vellum/macros.h>
#include <vellum/path.h>
#include <vellum/shell.h>

static int load_folios_handler(struct shell_instance *inst, int argc, char **argv)
{
    VlStatus status;
    struct elf_file *elf = NULL;
    void *load_paddr;
    size_t program_size;
    struct StLoad_BootInfoTableHeader *btblhdr;
    struct Lf_RamdiskImage ramdisk = {0};
    const char *ramdisk_path = NULL;
    char **kernel_argv = NULL;
    int kernel_argc = 0;

    if (argc < 2) {
        fprintf(stderr, "usage: %s kernel-path -ramdisk path [kernel args...]\n", argv[0]);
        return 1;
    }

    kernel_argv = malloc(sizeof(*kernel_argv) * argc);
    if (!kernel_argv) return 1;

    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "-ramdisk") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "%s: -ramdisk requires a path\n", argv[0]);
                free(kernel_argv);
                return 1;
            }
            ramdisk_path = argv[++i];
            continue;
        }

        if (strncmp(argv[i], "-ramdisk=", sizeof("-ramdisk=") - 1) == 0) {
            ramdisk_path = argv[i] + sizeof("-ramdisk=") - 1;
            continue;
        }

        kernel_argv[kernel_argc++] = argv[i];
    }

    if (!ramdisk_path || ramdisk_path[0] == '\0') {
        fprintf(stderr, "%s: -ramdisk is required\n", argv[0]);
        fprintf(stderr, "usage: %s kernel-path -ramdisk path [kernel args...]\n", argv[0]);
        free(kernel_argv);
        return 1;
    }

    char path[PATH_MAX];
    if (VlPath_IsAbsolute(argv[1])) {
        if (strlen(argv[1]) >= sizeof(path)) {
            fprintf(stderr, "%s: kernel path is too long\n", argv[0]);
            free(kernel_argv);
            return 1;
        }
        strncpy(path, argv[1], sizeof(path) - 1);
        path[sizeof(path) - 1] = '\0';
    } else {
        if (!inst->fs) {
            fprintf(stderr, "%s: filesystem not selected\n", argv[0]);
            free(kernel_argv);
            return 1;
        }
        if (strlen(inst->working_dir_path) >= sizeof(path)) {
            fprintf(stderr, "%s: working directory path is too long\n", argv[0]);
            free(kernel_argv);
            return 1;
        }
        if (strlen(inst->working_dir_path) + 1 + strlen(argv[1]) >= sizeof(path)) {
            fprintf(stderr, "%s: kernel path is too long\n", argv[0]);
            free(kernel_argv);
            return 1;
        }

        strncpy(path, inst->working_dir_path, sizeof(path) - 1);
        path[sizeof(path) - 1] = '\0';
        VlPath_Join(path, sizeof(path), argv[1]);
    }

    printf("Loading kernel...\n");

    status = Lf_LoadKernel(path, argv[0], &elf, &load_paddr, &program_size);
    if (!CHECK_SUCCESS(status)) {
        fprintf(stderr, "%s: failed to load kernel: 0x%08" PRIX32 "\n", argv[0], status);
        free(kernel_argv);
        return 1;
    }

    printf("Building ramdisk...\n");
    status = Lf_BuildRamdisk(inst, ramdisk_path, &ramdisk);
    if (!CHECK_SUCCESS(status)) {
        fprintf(stderr, "%s: failed to build ramdisk\n", argv[0]);
        free(kernel_argv);
        return 1;
    }

    status = Lf_MakeBootInfoTable(
        elf,
        program_size,
        argv[0],
        kernel_argc,
        kernel_argv,
        load_paddr,
        &ramdisk,
        &btblhdr
    );
    if (!CHECK_SUCCESS(status)) {
        fprintf(stderr, "%s: failed to build bootinfo table: 0x%08" PRIX32 "\n", argv[0], status);
        free(kernel_argv);
        return 1;
    }

    free(kernel_argv);

    Lf_PrepareKernelHandoff();
    Lf_JumpKernel(
        (void *)(uintptr_t)(elf->ident.class == ELFCLASS32 ? elf->ehdr32.entry : elf->ehdr64.entry),
        btblhdr
    );
}

static struct command load_folios_command = {
    .name = "load_folios",
    .handler = load_folios_handler,
    .help_message = "Load Strata kernel",
};

__constructor static void init()
{
    VlShell_RegisterCommand(&load_folios_command);
}

VlStatus _start(int argc, char **argv)
{
    return STATUS_SUCCESS;
}

__destructor static void deinit(void)
{
    VlShell_UnregisterCommand(&load_folios_command);
}
