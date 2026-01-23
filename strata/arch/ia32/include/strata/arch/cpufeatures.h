#ifndef __STRATA_ARCH_CPUFEATURES_H__
#define __STRATA_ARCH_CPUFEATURES_H__

#include <stddef.h>

#include <strata/status.h>
#include <strata/mm.h>

struct StA_CpuFeatures {
    uint32_t has_invlpg : 1;
    uint32_t has_cpuid : 1;
    uint32_t has_fpu : 1;
    uint32_t has_vme : 1;
    uint32_t has_de : 1;
    uint32_t has_pse : 1;
    uint32_t has_tsc : 1;
    uint32_t has_msr : 1;

    uint32_t has_pae : 1;
    uint32_t has_cx8 : 1;
    uint32_t has_apic : 1;
    uint32_t has_sep : 1;
    uint32_t has_pge : 1;
    uint32_t has_pse36 : 1;
    uint32_t has_pcid : 1;
    uint32_t has_dca : 1;

    uint32_t has_mmx : 1;
    uint32_t has_sse : 1;
    uint32_t has_sse2 : 1;
    uint32_t has_sse3 : 1;
    uint32_t has_ssse3 : 1;
    uint32_t has_sse4_1 : 1;
    uint32_t has_sse4_2 : 1;
    uint32_t has_movbe : 1;

    uint32_t has_popcnt : 1;
    uint32_t has_aes_ni : 1;
    uint32_t has_xsave : 1;
    uint32_t has_osxsave : 1;
    uint32_t has_avx : 1;
    uint32_t has_f16c : 1;
    uint32_t has_nx : 1;
    uint32_t has_lm: 1;

    uint32_t has_pdpe1gb: 1;
    uint32_t has_syscall: 1;
    uint32_t has_rdtscp: 1;
    uint32_t has_lahf_lm: 1;
    uint32_t has_abm: 1;
    uint32_t has_cx16: 1;
    uint32_t has_pat: 1;
    uint32_t has_cmov: 1;

    uint32_t has_clfsh: 1;
    uint32_t has_fma: 1;
    uint32_t has_htt: 1;
    uint32_t has_fxsr: 1;
    uint32_t has_x2apic: 1;
};

extern const struct StA_CpuFeatures *const g_p_cpu_features;

StStatus StA_CheckCpuFeatures(void);

StStatus StA_ActivateCommonCpuFeatures(void);

#endif // __STRATA_ARCH_CPUFEATURES_H__
