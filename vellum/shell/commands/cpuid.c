#include <vellum/shell.h>

#include <stdio.h>

#include <vellum/arch/intrinsics/cpuid.h>

static int check_cpuid_available(void)
{
    int available;
    __asm__ volatile("pushfl\n\t"
                     "pushfl\n\t"
                     "xor $0x00200000, (%%esp)\n\t"
                     "popfl\n\t"
                     "pushfl\n\t"
                     "pop %%eax\n\t"
                     "xor (%%esp), %%eax\n\t"
                     "popfl\n\t"
                     "and $0x00200000, %%eax\n\t"
                     : "=r"(available)
                     :
                     : "eax");

    return !!available;
}

static int cpuid_handler(struct shell_instance *inst, int argc, char **argv)
{
    uint32_t max_param;

    if (!check_cpuid_available()) {
        fprintf(stderr, "%s: cpuid instruction is unavailable\n", argv[0]);
        return 1;
    }

    uint32_t eax, ebx, ecx, edx;
    VlA_Cpuid(0, &eax, &ebx, &ecx, &edx);

    max_param = eax;
    uint32_t vendor_str[3] = {ebx, edx, ecx};
    printf("Vendor String: %12s\n", (char *)vendor_str);

    if (max_param >= 1) {
        VlA_Cpuid(1, &eax, &ebx, &ecx, &edx);

        printf("Processor Type: %1lX\n", (eax & 0x00003000) >> 12);
        printf("Model ID: %02lX\n", ((eax & 0x000F0000) >> 12) | ((eax & 0x000000F0) >> 4));
        printf("Family ID: %03lX\n", ((eax & 0x0FF00000) >> 16) | ((eax & 0x00000F00) >> 8));
        printf("Stepping ID: %1lX\n", eax & 0x0000000F);

        printf("Brand Index: %02lX\n", ebx & 0x000000FF);
        if (edx & 0x00080000) {
            printf("CFLUSH Line Size: %ld\n", (ebx & 0x0000FF00) >> 5);
        }
        if (edx & 0x10000000) {
            printf("Max Logical Processor ID: %ld\n", (ebx & 0x00FF0000) >> 16);
        }
        if (edx & 0x00000200) {
            printf("Local APIC ID: %ld\n", (ebx & 0xFF000000) >> 16);
        }

        printf("CPU Features: ");

        for (int i = 0; i < 32; i++) {
            if ((ecx >> i) & 1) {
                static const char *features[] = {
                    "SSE3",   "PCLMUL", "DTES64", "MONITOR",    "DS_CPL", "VMX",    "SMX",
                    "EST",    "TM2",    "SSSE3",  "CID",        "SDBG",   "FMA",    "CX16",
                    "XTPR",   "PDCM",   NULL,     "PCID",       "DCA",    "SSE4.1", "SSE4.2",
                    "X2APIC", "MOVBE",  "POPCNT", "TSC",        "AES",    "XSAVE",  "OSXSAVE",
                    "AVX",    "F16C",   "RDRAND", "HYPERVISOR",
                };
                if (!features[i]) continue;

                printf("%s ", features[i]);
            }
        }

        for (int i = 0; i < 32; i++) {
            if ((edx >> i) & 1) {
                static const char *features[] = {
                    "FPU",  "VME",   "DE",   "PSE",     "TSC",  "MSR", "PAE",  "MCE",
                    "CX8",  "APIC",  NULL,   "SEP",     "MTRR", "PGE", "MCA",  "CMOV",
                    "PAT",  "PSE36", "PSN",  "CLFLUSH", NULL,   "DS",  "ACPI", "MMX",
                    "FXSR", "SSE",   "SSE2", "SS",      "HTT",  "TM",  "IA64", "PBE",
                };
                if (!features[i]) continue;

                printf("%s ", features[i]);
            }
        }

        printf("\n");
    }

    VlA_Cpuid(0x80000000, &eax, &ebx, &ecx, &edx);
    max_param = eax;

    if (max_param >= 0x80000001) {
        printf("CPU Features (More): ");

        for (int i = 0; i < 32; i++) {
            if ((ecx >> i) & 1) {
                static const char *features[] = {
                    "LAHF_LM",
                    "CMP_LEGACY",
                    "SVM",
                    "EXTAPIC",
                    "CR8_LEGACY",
                    "ABM_LZCNT",
                    "SSE4A",
                    "MISALIGNSSE",
                    "3DNOWPREFETCH",
                    "OSVW",
                    "IBS",
                    "XOP",
                    "SKINIT",
                    "WDT",
                    NULL,
                    "LWP",
                    "FMA4",
                    "TCE",
                    NULL,
                    "NODEID_MSR",
                    NULL,
                    "TBM",
                    "TOPOEXT",
                    "PERFCTR_CORE",
                    "PERFCTR_NB",
                    "STREAMPERFMON",
                    "DBX",
                    "PERFTSC",
                    "PCX_L2I",
                    "MONITORX",
                    "ADDR_MASK_EXT",
                    NULL,
                };
                if (!features[i]) continue;

                printf("%s ", features[i]);
            }
        }

        for (int i = 0; i < 32; i++) {
            if ((edx >> i) & 1) {
                static const char *features[] = {
                    NULL,   NULL,       NULL,      NULL,      NULL, NULL, NULL,       NULL,
                    NULL,   NULL,       "SYSCALL", "SYSCALL", NULL, NULL, NULL,       NULL,
                    NULL,   NULL,       NULL,      "ECC",     "NX", NULL, "MMXEXT",   NULL,
                    "FXSR", "FXSR_OPT", "PDPE1GB", "RDTSCP",  NULL, "LM", "3DNOWEXT", "3DNOW"
                };
                if (!features[i]) continue;

                printf("%s ", features[i]);
            }
        }

        printf("\n");
    }

    if (max_param >= 0x80000004) {
        uint32_t brand_str[12];

        VlA_Cpuid(0x80000002, &brand_str[0], &brand_str[1], &brand_str[2], &brand_str[3]);
        VlA_Cpuid(0x80000003, &brand_str[4], &brand_str[5], &brand_str[6], &brand_str[7]);
        VlA_Cpuid(0x80000004, &brand_str[8], &brand_str[9], &brand_str[10], &brand_str[11]);

        printf("Brand String: %48s\n", (char *)brand_str);
    }

    return 0;
}

static struct command cpuid_command = {
    .name = "cpuid",
    .handler = cpuid_handler,
    .help_message = "Invoke CPUID",
};

static void cpuid_command_init(void)
{
    VlShell_RegisterCommand(&cpuid_command);
}

REGISTER_SHELL_COMMAND(cpuid, cpuid_command_init)
