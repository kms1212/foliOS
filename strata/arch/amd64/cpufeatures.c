#include <strata/arch/cpufeatures.h>

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

#include <cpuid.h>

#include <strata/arch/intrinsics/cpuid.h>
#include <strata/arch/intrinsics/msr.h>
#include <strata/arch/intrinsics/register.h>

#include <strata/log.h>
#include <strata/panic.h>
#include <strata/status.h>

#define MODULE_NAME "cpufeat"

static struct StA_CpuFeatures cpu_features;
const struct StA_CpuFeatures *const g_p_cpu_features = &cpu_features;

static int check_cpuid_available(void)
{
    int available;

    __asm__ volatile("pushfq\n\t"
                     "pushfq\n\t"
                     "xorl   $0x00200000, (%%rsp)\n\t"
                     "popfq\n\t"
                     "pushfq\n\t"
                     "pop    %%rax\n\t"
                     "xor    (%%rsp), %%rax\n\t"
                     "popfq\n\t"
                     "and    $0x00200000, %%rax\n\t"
                     : "=r"(available)
                     :
                     : "rax");

    return !!available;
}

StStatus StA_CheckCpuFeatures(void)
{
    uint32_t max_param, max_param_ext, eax, ebx, ecx, edx;

    if (!check_cpuid_available()) {
        St_Panic(STATUS_NOT_SUPPORTED, "CPUID instruction is not available");
    }

    cpu_features.has_cpuid = 1;
    cpu_features.has_invlpg = 1;

    StA_Cpuid(CPUID_VENDOR_STRING, &eax, &ebx, &ecx, &edx);
    max_param = eax;
    LOG_DEBUG(
        LM_CAT_UNCLASSIFIED,
        "vendor string: %.4s%.4s%.4s\n",
        (char *)&ebx,
        (char *)&edx,
        (char *)&ecx
    );

    if (max_param >= CPUID_FEATURES) {
        StA_Cpuid(CPUID_FEATURES, &eax, &ebx, &ecx, &edx);

        LOG_DEBUG(LM_CAT_UNCLASSIFIED, "processor type: %1" PRIX32 "\n", (eax & 0x00003000) >> 12);
        LOG_DEBUG(
            LM_CAT_UNCLASSIFIED,
            "model id: %02" PRIX32 "\n",
            ((eax & 0x000F0000) >> 12) | ((eax & 0x000000F0) >> 4)
        );
        LOG_DEBUG(
            LM_CAT_UNCLASSIFIED,
            "family id: %03" PRIX32 "\n",
            ((eax & 0x0FF00000) >> 16) | ((eax & 0x00000F00) >> 8)
        );
        LOG_DEBUG(LM_CAT_UNCLASSIFIED, "stepping id: %1" PRIX32 "\n", eax & 0x0000000F);

        LOG_DEBUG(LM_CAT_UNCLASSIFIED, "branding index: %02" PRIX32 "\n", ebx & 0x000000FF);

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

        if (ecx & (1 << 17)) {
            cpu_features.has_pcid = 1;
        }

        if (ecx & (1 << 18)) {
            cpu_features.has_dca = 1;
        }

        if (ecx & bit_SSE4_1) {
            cpu_features.has_sse4_1 = 1;
        }

        if (ecx & bit_SSE4_2) {
            cpu_features.has_sse4_2 = 1;
        }

        if (ecx & (1 << 21)) {
            cpu_features.has_x2apic = 1;
        }

        if (ecx & bit_MOVBE) {
            cpu_features.has_movbe = 1;
        }

        if (ecx & bit_POPCNT) {
            cpu_features.has_popcnt = 1;
        }

        if (ecx & (1 << 25)) {
            cpu_features.has_aes_ni = 1;
        }

        if (ecx & bit_XSAVE) {
            cpu_features.has_xsave = 1;
        }

        /*
         * Ignore CPUID.OSXSAVE here. It reports whether the current software
         * environment has already enabled CR4.OSXSAVE, not an independent CPU
         * capability. Strata decides that during activation.
         */

        if (ecx & bit_AVX) {
            cpu_features.has_avx = 1;
        }

        if (ecx & bit_F16C) {
            cpu_features.has_f16c = 1;
        }

        if (edx & (1 << 28)) {
            cpu_features.has_htt = 1;
            LOG_DEBUG(
                LM_CAT_UNCLASSIFIED,
                "max logical processor id: %" PRId32 "\n",
                (ebx & 0x00FF0000) >> 16
            );
        }

        if (edx & (1 << 9)) {
            LOG_DEBUG(
                LM_CAT_UNCLASSIFIED,
                "local APIC id: %" PRId32 "\n",
                (ebx & 0xFF000000) >> 16
            );
        }

        if (edx & (1 << 0)) {
            cpu_features.has_fpu = 1;
        }

        if (edx & (1 << 1)) {
            cpu_features.has_vme = 1;
        }

        if (edx & (1 << 2)) {
            cpu_features.has_de = 1;
        }

        if (edx & (1 << 3)) {
            cpu_features.has_pse = 1;
        }

        if (edx & (1 << 4)) {
            cpu_features.has_tsc = 1;
        }

        if (edx & (1 << 5)) {
            cpu_features.has_msr = 1;
        }

        if (edx & (1 << 6)) {
            cpu_features.has_pae = 1;
        }

        if (edx & bit_CMPXCHG8B) {
            cpu_features.has_cx8 = 1;
        }

        if (edx & (1 << 9)) {
            cpu_features.has_apic = 1;
        }

        if (edx & (1 << 11)) {
            cpu_features.has_sep = 1;
        }

        if (edx & (1 << 13)) {
            cpu_features.has_pge = 1;
        }

        if (edx & bit_CMOV) {
            cpu_features.has_cmov = 1;
        }

        if (edx & (1 << 16)) {
            cpu_features.has_pat = 1;
        }

        if (edx & (1 << 17)) {
            cpu_features.has_pse36 = 1;
        }

        if (edx & (1 << 19)) {
            cpu_features.has_clfsh = 1;
            LOG_DEBUG(
                LM_CAT_UNCLASSIFIED,
                "CLFSH line size: %" PRId32 "\n",
                (ebx & 0x0000FF00) >> 5
            );
        }

        if (edx & bit_MMX) {
            cpu_features.has_mmx = 1;
        }

        if (edx & (1 << 24)) {
            cpu_features.has_fxsr = 1;
        }

        if (edx & bit_SSE) {
            cpu_features.has_sse = 1;
        }

        if (edx & bit_SSE2) {
            cpu_features.has_sse2 = 1;
        }
    }

    if (max_param >= CPUID_TSC_INFO) {
        StA_Cpuid(CPUID_TSC_INFO, &eax, &ebx, &ecx, &edx);

        cpu_features.tsc_ratio_denom = eax;
        if (ebx) {
            cpu_features.provides_tsc_ratio = 1;
            cpu_features.tsc_ratio_numer = ebx;
        }

        if (ecx) {
            cpu_features.provides_core_clock_freq = 1;
            cpu_features.core_clock_freq_hz = ecx;
        }
    }

    StA_Cpuid(CPUID_INTEL_EXTENDED, &eax, &ebx, &ecx, &edx);
    max_param_ext = eax;

    if (max_param_ext >= CPUID_INTEL_FEATURES) {
        StA_Cpuid(CPUID_INTEL_FEATURES, &eax, &ebx, &ecx, &edx);

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

    if (max_param_ext >= CPUID_INTEL_PM_INFO_RAS_CAPS) {
        StA_Cpuid(CPUID_INTEL_PM_INFO_RAS_CAPS, &eax, &ebx, &ecx, &edx);

        if (edx & (1 << 8)) {
            cpu_features.is_tsc_invariant = 1;
        }
    }

    return STATUS_SUCCESS;
}

StStatus StA_ActivateCommonCpuFeatures(void)
{
    uint64_t cr0 = StA_ReadCr0();
    uint64_t cr4 = StA_ReadCr4();

    if (cpu_features.has_fpu) {
        cr0 |= 0x0000000000010022;  /* turn on MP, NE, WP */
        cr0 &= ~0x0000000000000004; /* turn off EM */
    }

    /* debugging extensions */
    if (cpu_features.has_de) {
        cr4 |= 0x0000000000000008;
    }

    /* global page */
    if (cpu_features.has_pge) {
        cr4 |= 0x0000000000000080;
    }

    /* fxsave/fxrstor */
    if (cpu_features.has_fxsr) {
        cr4 |= 0x0000000000000200;
    }

    /* osxmmexcpt */
    if (cpu_features.has_sse) {
        cr4 |= 0x0000000000000400;
    }

    /* osxsave */
    if (cpu_features.has_xsave) {
        cr4 |= 0x0000000000040000;
    }

    StA_WriteCr0(cr0);
    StA_WriteCr4(cr4);

    uint64_t efer = StA_ReadMsr(MSR_EFER);

    /* no-execute bit */
    if (cpu_features.has_nx) {
        efer |= EFER_NXE;
    }

    /* syscall */
    if (cpu_features.has_msr && cpu_features.has_syscall) {
        efer |= EFER_SCE;
    }

    StA_WriteMsr(MSR_EFER, efer);

    if (cpu_features.has_xsave) {
        uint64_t xcr0 = StA_ReadXcr0();

        /*
         * Enable x87/SSE state for XSAVE. Add AVX state when available.
         */
        xcr0 |= 0x0000000000000003;
        if (cpu_features.has_avx) {
            xcr0 |= 0x0000000000000004;
        }
        StA_WriteXcr0(xcr0);
    }

    return STATUS_SUCCESS;
}
