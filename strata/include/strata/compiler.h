#ifndef __STRATA_COMPILER_H__
#define __STRATA_COMPILER_H__

#ifndef __has_attribute
#    define __has_attribute(x) 0

#endif

/* General attribute maros */

#define __always_inline inline __attribute__((always_inline))

#ifndef __unused
#    define __unused __attribute__((unused))

#endif

#define __noreturn                __attribute__((noreturn))
#define __naked                   __attribute__((naked))
#define __packed                  __attribute__((packed))
#define __format_printf(fmt, chk) __attribute__((format(printf, fmt, chk)))
#define __aligned(n)              __attribute__((aligned(n)))
#define __constructor             __attribute__((constructor))
#define __destructor              __attribute__((destructor))
#define __section(s)              __attribute__((section(#s)))
#define __optimize(s)             __attribute__((optimize(s)))
#define __target(s)               __attribute__((target(s)))

#ifndef __weak
#    define __weak __attribute__((weak))

#endif

#define __hot __attribute__((hot))

#ifndef __cold
#    define __cold __attribute__((cold))

#endif

#ifndef __pure
#    define __pure __attribute__((pure))

#endif

#ifndef __deprecated
#    define __deprecated __attribute__((deprecated))

#endif

#ifdef __clang__
#    define __externally_visible

#else
#    define __externally_visible __attribute__((externally_visible))

#endif

#define __sentinel           __attribute__((sentinel))
#define __warn_unused_result __attribute__((warn_unused_result))

#if __has_attribute(nonnull)
#    define __nonnull(...) __attribute__((nonnull(__VA_ARGS__)))
#    define __arg_nonnull __attribute__((nonnull))

#else
#    define __nonnull(...)
#    define __arg_nonnull

#endif

#if __has_attribute(returns_nonnull)
#    define __returns_nonnull __attribute__((returns_nonnull))

#else
#    define __returns_nonnull

#endif

#if __has_attribute(alloc_size)
#    define __alloc_size(...) __attribute__((alloc_size(__VA_ARGS__)))

#else
#    define __alloc_size(...)

#endif

#if __has_attribute(alloc_align)
#    define __alloc_align(arg_index) __attribute__((alloc_align(arg_index)))

#else
#    define __alloc_align(arg_index)

#endif

#if __has_attribute(assume_aligned)
#    define __assume_aligned(align) __attribute__((assume_aligned(align)))

#else
#    define __assume_aligned(align)

#endif

#if __has_attribute(access)
#    define __access_read_only(ptr_index, size_index)                                           \
        __attribute__((access(read_only, ptr_index, size_index)))
#    define __access_write_only(ptr_index, size_index)                                          \
        __attribute__((access(write_only, ptr_index, size_index)))
#    define __access_read_write(ptr_index, size_index)                                          \
        __attribute__((access(read_write, ptr_index, size_index)))

#else
#    define __access_read_only(ptr_index, size_index)
#    define __access_write_only(ptr_index, size_index)
#    define __access_read_write(ptr_index, size_index)

#endif

/* macros for static analysis & source annotation */

#ifdef __clang__
#    define __annotate(name)      __attribute__((annotate("st_" #name)))
#    define __annotate_v(name, v) __attribute__((annotate("st_" #name "=" #v)))
#    define __ref_annotate(name)  __attribute__((annotate(#name)))

#else
#    define __annotate(name)
#    define __annotate_v(name, v)
#    define __ref_annotate(name)

#endif

#define __in           __annotate("in")
#define __out          __annotate("out")
#define __out_optional __annotate("out_optional")
#define __inout        __annotate("inout")
#define __buf          __annotate("buf")

#define __kernel __annotate("kernel")
#define __percpu __annotate("percpu")
#define __iomem  __annotate("iomem")
#define __module __annotate("module")
#define __user   __annotate("user")

#if __has_attribute(capability)
#    define __capability(name) __attribute__((capability(name)))
#    define __guarded_by(x)    __attribute__((guarded_by(x)))
#    define __pt_guarded_by(x) __attribute__((pt_guarded_by(x)))

#else
#    define __capability(name) __annotate_v("capability", name)
#    define __guarded_by(x)    __annotate_v("guarded_by", x)
#    define __pt_guarded_by(x) __annotate_v("pt_guarded_by", x)

#endif

#if __has_attribute(requires_capability)
#    define __requires_capability(x) __attribute__((requires_capability(x)))

#else
#    define __requires_capability(x) __annotate_v("must_hold", x)

#endif

#if __has_attribute(acquire_capability)
#    define __acquire_capability(x) __attribute__((acquire_capability(x)))

#else
#    define __acquire_capability(x) __annotate_v("acquires", x)

#endif

#if __has_attribute(try_acquire_capability)
#    define __try_acquire_capability(x) __attribute__((try_acquire_capability(1, x)))

#else
#    define __try_acquire_capability(x) __annotate_v("cond_acquires", x)

#endif

#if __has_attribute(release_capability)
#    define __release_capability(x) __attribute__((release_capability(x)))

#else
#    define __release_capability(x) __annotate_v("releases", x)

#endif

#if __has_attribute(no_thread_safety_analysis)
#    define __no_thread_safety_analysis __attribute__((no_thread_safety_analysis))

#else
#    define __no_thread_safety_analysis

#endif

#define __must_hold(x)     __requires_capability(x)
#define __acquires(x)      __acquire_capability(x)
#define __cond_acquires(x) __try_acquire_capability(x)
#define __releases(x)      __release_capability(x)

#define __bitwise __annotate("bitwise")
#define __nocast  __annotate("nocast")

#define __ref_strong   __ref_annotate(ref_strong)
#define __ref_weak     __ref_annotate(ref_weak)
#define __ref_borrowed __ref_annotate(ref_borrowed)
#define __ref_internal __ref_annotate(ref_internal)

#endif  // __STRATA_COMPILER_H__
