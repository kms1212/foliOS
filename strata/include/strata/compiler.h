#ifndef __STRATA_COMPILER_H__
#define __STRATA_COMPILER_H__

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

/* macros for static analysis & source annotation */

#ifdef __clang__
#    define __annotate(name)      __attribute__((annotate("st_" #name)))
#    define __annotate_v(name, v) __attribute__((annotate("st_" #name "=" #v)))

#else
#    define __annotate(name)
#    define __annotate_v(name, v)

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

#define __must_hold(x)     __annotate_v("must_hold", x)
#define __acquires(x)      __annotate_v("acquires", x)
#define __cond_acquires(x) __annotate_v("cond_acquires", x)
#define __releases(x)      __annotate_v("releases", x)

#define __bitwise __annotate("bitwise")
#define __nocast  __annotate("nocast")

#endif  // __STRATA_COMPILER_H__
