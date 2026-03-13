#ifndef __VELLUM_ARCH_CPUFEATURES_H__
#define __VELLUM_ARCH_CPUFEATURES_H__

#include <stddef.h>

#include <vellum/status.h>

struct VlA_CpuFeatures {
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
    uint32_t has_lm : 1;

    uint32_t has_pdpe1gb : 1;
    uint32_t has_syscall : 1;
    uint32_t has_rdtscp : 1;
    uint32_t has_lahf_lm : 1;
    uint32_t has_abm : 1;
    uint32_t has_cx16 : 1;
    uint32_t has_pat : 1;
    uint32_t has_cmov : 1;

    uint32_t has_clfsh : 1;
    uint32_t has_fma : 1;
    uint32_t has_htt : 1;
    uint32_t has_fxsr : 1;
    uint32_t has_x2apic : 1;
    uint32_t has_fsgsbase : 1;
    uint32_t has_bmi1 : 1;
    uint32_t has_avx2 : 1;

    uint32_t has_smep : 1;
    uint32_t has_bmi2 : 1;
    uint32_t has_erms : 1;
    uint32_t has_invpcid : 1;
    uint32_t has_mpx : 1;
    uint32_t has_avx512_f : 1;
    uint32_t has_avx512_dq : 1;
    uint32_t has_rdseed : 1;

    uint32_t has_smap : 1;
    uint32_t has_avx512_ifma : 1;
    uint32_t has_clflushopt : 1;
    uint32_t has_clwb : 1;
    uint32_t has_avx512_pf : 1;
    uint32_t has_avx512_er : 1;
    uint32_t has_avx512_cd : 1;
    uint32_t has_sha : 1;

    uint32_t has_avx512_bw : 1;
    uint32_t has_avx512_vl : 1;
    uint32_t has_avx512_vbmi : 1;
    uint32_t has_avx512_umip : 1;
    uint32_t has_pku : 1;
    uint32_t has_ospke : 1;
    uint32_t has_avx512_vbmi2 : 1;
    uint32_t has_vaes : 1;

    uint32_t has_avx512_vnni : 1;
    uint32_t has_avx512_bitalg : 1;
    uint32_t has_avx512_vpopcntdq : 1;
    uint32_t has_la57 : 1;
    uint32_t has_fsrm : 1;
    uint32_t has_uintr : 1;
    uint32_t has_sha512 : 1;
    uint32_t has_fzrm : 1;

    uint32_t has_fsrs : 1;
    uint32_t has_rsrcs : 1;
    uint32_t has_vmx : 1;
    uint32_t has_svm : 1;
    uint32_t has_cet_ss : 1;
    uint32_t has_cet_ibt : 1;
    uint32_t is_hybrid : 1;
    uint32_t has_waitpkg : 1;

    uint32_t has_clzero : 1;
    uint32_t has_lam : 1;
    uint32_t provides_tsc_ratio : 1;
    uint32_t provides_core_clock_freq : 1;
    uint32_t is_tsc_invariant : 1;

    uint32_t tsc_ratio_denom;
    uint32_t tsc_ratio_numer;
    uint32_t core_clock_freq_hz;
};

extern const struct VlA_CpuFeatures *const g_p_cpu_features;

status_t VlA_CheckCpuFeatures(void);

#endif  // __VELLUM_ARCH_CPUFEATURES_H__
