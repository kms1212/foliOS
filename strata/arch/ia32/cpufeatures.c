#include <strata/arch/cpufeatures.h>

#include <stdio.h>
#include <inttypes.h>

#include <cpuid.h>

#include <strata/arch/intrinsics/cpuid.h>
#include <strata/arch/intrinsics/register.h>

#include <strata/plat/interrupt.h>

#include <strata/panic.h>
#include <strata/log.h>

#define MODULE_NAME "cpufeatures"

/* additional bits not defined int cpuid.h */

/* leaf 0x00000000, ecx */
#define bit_PCID (1 << 17)
#define bit_DCA (1 << 18)
#define bit_x2APIC (1 << 21)
#define bit_AESNI (1 << 25)

/* leaf 0x00000000, edx */
#define bit_FPU (1 << 0)
#define bit_VME (1 << 1)
#define bit_APIC (1 << 9)
#define bit_HTT (1 << 28)
#define bit_DE (1 << 2)
#define bit_PSE (1 << 3)
#define bit_TSC (1 << 4)
#define bit_MSR (1 << 5)
#define bit_PAE (1 << 6)
#define bit_SEP (1 << 11)
#define bit_PGE (1 << 13)
#define bit_PAT (1 << 16)
#define bit_PSE36 (1 << 17)
#define bit_CLFSH (1 << 19)
#define bit_FXSR (1 << 24)


static volatile int handler_called;
static size_t instr_size;

static void *fault_handler_func(int, struct StA_InterruptFrame *frame, struct StIntP_Context *ctx, void *)
{
    handler_called = 1;
    frame->eip += instr_size;

    return NULL;
}

static StStatus test_instruction(void (*test_func)(void), size_t _instr_size, int *is_undefined)
{
    StStatus status;
    uint32_t intstate;
    struct StInt_Handler *orig_first_handler = NULL;
    struct StInt_Handler temp_handler = {
        .next = NULL,
        .irq_num = 0x06,
        .data = NULL,
        .handler = fault_handler_func,
    };

    intstate = StA_SaveInterrupt();
    StA_DisableInterrupt();

    status = StIntP_GetFirstHandler(0x06, &orig_first_handler);
    if (!CHECK_SUCCESS(status)) goto has_error;
    
    handler_called = 0;
    instr_size = _instr_size;

    status = StIntP_SetFirstHandler(0x06, &temp_handler);
    if (!CHECK_SUCCESS(status)) goto has_error;

    test_func();

    if (is_undefined) *is_undefined = handler_called;

    status = StIntP_SetFirstHandler(0x06, orig_first_handler);
    if (!CHECK_SUCCESS(status)) goto has_error;

    StA_RestoreInterrupt(intstate);
    
    return STATUS_SUCCESS;
    
has_error:
    if (orig_first_handler && !CHECK_SUCCESS(StIntP_SetFirstHandler(0x06, orig_first_handler))) {
        St_Panic(status, "failed to restore interrupt handler while recovering from failure");
    }

    StA_RestoreInterrupt(intstate);

    return status;
}

static struct StA_CpuFeatures cpu_features;
const struct StA_CpuFeatures *const g_p_cpu_features = &cpu_features;

static int check_cpuid_available(void)
{
    int available;
    
    __asm__ volatile (
        "pushfq\n\t"
        "pushfq\n\t"
        "xorl   $0x00200000, (%%rsp)\n\t"
        "popfq\n\t"
        "pushfq\n\t"
        "pop    %%rax\n\t"
        "xor    (%%rsp), %%rax\n\t"
        "popfq\n\t"
        "and    $0x00200000, %%rax\n\t"
        : "=r"(available) : : "rax"
    );

    return !!available;
}

StStatus StA_CheckCpuFeatures(void)
{
    uint32_t max_param, max_param_ext, eax, ebx, ecx, edx;

    if (!check_cpuid_available()) {
        St_Panic(STATUS_UNSUPPORTED, "CPUID instruction is not available");
    }

    cpu_features.has_cpuid = 1;
    cpu_features.has_invlpg = 1;

    StA_Cpuid(0x00000000, &eax, &ebx, &ecx, &edx);
    max_param = eax;
    LOG_DEBUG("vendor string: %4s%4s%4s\n", (char *)&ebx, (char *)&ecx, (char *)&edx);

    if (max_param >= 1) {
        StA_Cpuid(0x00000001, &eax, &ebx, &ecx, &edx);

        LOG_DEBUG("processor type: %1"PRIX32"\n", (eax & 0x00003000) >> 12);
        LOG_DEBUG("model id: %02"PRIX32"\n", ((eax & 0x000F0000) >> 12) | ((eax & 0x000000F0) >> 4));
        LOG_DEBUG("family id: %03"PRIX32"\n", ((eax & 0x0FF00000) >> 16) | ((eax & 0x00000F00) >> 8));
        LOG_DEBUG("stepping id: %1"PRIX32"\n", eax & 0x0000000F);

        LOG_DEBUG("branding index: %02"PRIX32"\n", ebx & 0x000000FF);

        if (ecx & bit_SSE3) {
            cpu_features.has_sse3 = 1;
        }

        if (ecx & bit_SSSE3) {
            cpu_features.has_ssse3 = 1;
        }

        if (ecx & bit_FMA) {
            cpu_features.has_fma = 1;
        }

        if (edx & bit_CMPXCHG16B) {
            cpu_features.has_cx16 = 1;
        }

        if (ecx & bit_PCID) {
            cpu_features.has_pcid = 1;
        }

        if (ecx & bit_DCA) {
            cpu_features.has_dca = 1;
        }

        if (ecx & bit_SSE4_1) {
            cpu_features.has_sse4_1 = 1;
        }

        if (ecx & bit_SSE4_2) {
            cpu_features.has_sse4_2 = 1;
        }

        if (ecx & bit_x2APIC) {
            cpu_features.has_x2apic = 1;
        }

        if (ecx & bit_MOVBE) {
            cpu_features.has_movbe = 1;
        }

        if (ecx & bit_POPCNT) {
            cpu_features.has_popcnt = 1;
        }

        if (ecx & bit_AESNI) {
            cpu_features.has_aes_ni = 1;
        }

        if (ecx & bit_XSAVE) {
            cpu_features.has_xsave = 1;
        }

        if (ecx & bit_OSXSAVE) {
            cpu_features.has_osxsave = 1;
        }

        if (ecx & bit_AVX) {
            cpu_features.has_avx = 1;
        }

        if (ecx & bit_F16C) {
            cpu_features.has_f16c = 1;
        }

        if (edx & bit_HTT) {
            cpu_features.has_htt = 1;
            LOG_DEBUG("max logical processor id: %"PRId32"\n", (ebx & 0x00FF0000) >> 16);
        }

        if (edx & bit_APIC) {
            LOG_DEBUG("local APIC id: %"PRId32"\n", (ebx & 0xFF000000) >> 16);
        }

        if (edx & bit_FPU) {
            cpu_features.has_fpu = 1;
        }

        if (edx & bit_VME) {
            cpu_features.has_vme = 1;
        }

        if (edx & bit_DE) {
            cpu_features.has_de = 1;
        }

        if (edx & bit_PSE) {
            cpu_features.has_pse = 1;
        }

        if (edx & bit_TSC) {
            cpu_features.has_tsc = 1;
        }

        if (edx & bit_MSR) {
            cpu_features.has_msr = 1;
        }

        if (edx & bit_PAE) {
            cpu_features.has_pae = 1;
        }

        if (edx & bit_CMPXCHG8B) {
            cpu_features.has_cx8 = 1;
        }

        if (edx & bit_APIC) {
            cpu_features.has_apic = 1;
        }

        if (edx & bit_SEP) {
            cpu_features.has_sep = 1;
        }

        if (edx & bit_PGE) {
            cpu_features.has_pge = 1;
        }

        if (edx & bit_CMOV) {
            cpu_features.has_cmov = 1;
        }

        if (edx & bit_PAT) {
            cpu_features.has_pat = 1;
        }

        if (edx & bit_PSE36) {
            cpu_features.has_pse36 = 1;
        }

        if (edx & bit_CLFSH) {
            cpu_features.has_clfsh = 1;
            LOG_DEBUG("CLFSH line size: %"PRId32"\n", (ebx & 0x0000FF00) >> 5);
        }

        if (edx & bit_MMX) {
            cpu_features.has_mmx = 1;
        }

        if (edx & bit_FXSR) {
            cpu_features.has_fxsr = 1;
        }

        if (edx & bit_SSE) {
            cpu_features.has_sse = 1;
        }

        if (edx & bit_SSE2) {
            cpu_features.has_sse2 = 1;
        }
    }

    StA_Cpuid(0x80000000, &eax, &ebx, &ecx, &edx);
    max_param_ext = eax;

    if (max_param_ext >= 0x80000001) {
        StA_Cpuid(0x80000001, &eax, &ebx, &ecx, &edx);

        if (edx & 0x00100000) {
            cpu_features.has_nx = 1;
        }

        /* TODO: check bit 10 instead of bit 11 on family 5 model 7 (AMD K6, 250nm "Little Foot") */
        if (edx & 0x00000800) {
            cpu_features.has_syscall = 1;
        }

        if (edx & 0x04000000) {
            cpu_features.has_pdpe1gb = 1;
        }

        if (edx & 0x08000000) {
            cpu_features.has_rdtscp = 1;
        }

        if (edx & 0x20000000) {
            cpu_features.has_lm = 1;
        }

        if (ecx & 0x00000001) {
            cpu_features.has_lahf_lm = 1;
        }

        if (ecx & 0x00000020) {
            cpu_features.has_abm = 1;
        }
    }

    return STATUS_SUCCESS;
}

StStatus StA_ActivateCommonCpuFeatures(void)
{
    uint32_t cr0 = StA_ReadCr0();
    uint32_t cr4 = StA_ReadCr4();

    if (cpu_features.has_fpu) {
        cr0 |= 0x00010022;  /* turn on MP, EM, NE, WP */
        cr0 &= ~0x00000004;  /* turn off EM */
    }

    /* debugging extensions */
    if (cpu_features.has_de) {
        cr4 |= 0x00000008;
    }

    /* global page */
    if (cpu_features.has_pge) {
        cr4 |= 0x00000080;
    }

    /* fxsave/fxrstor */
    if (cpu_features.has_fxsr) {
        cr4 |= 0x00000200;
    }

    /* osxmmexcpt */
    if (cpu_features.has_sse) {
        cr4 |= 0x00000400;
    }

    StA_WriteCr0(cr0);
    StA_WriteCr4(cr4);

    return STATUS_SUCCESS;
}
