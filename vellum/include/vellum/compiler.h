#ifndef __VELLUM_COMPILER_H__
#define __VELLUM_COMPILER_H__

#define __always_inline           inline __attribute__((always_inline))
#define __always_unused           __attribute__((unused))
#define __noreturn                __attribute__((noreturn))
#define __naked                   __attribute__((naked))
#define __packed                  __attribute__((packed))
#define __format_printf(fmt, chk) __attribute__((format(printf, fmt, chk)))
#define __format_scanf(fmt, chk)  __attribute__((format(scanf, fmt, chk)))
#define __malloc_like(free_func)  __attribute__((malloc, malloc(free_func, 1)))
#define __aligned(n)              __attribute__((aligned(n)))
#define __constructor             __attribute__((constructor))
#define __destructor              __attribute__((destructor))
#define __pure                    __attribute__((pure))

#ifndef likely
#    define likely(cond) __builtin_expect(!!(cond), 1)

#endif

#ifndef unlikely
#    define unlikely(cond) __builtin_expect(!!(cond), 0)

#endif

#define if_likely(cond)   if (likely(cond))
#define if_unlikely(cond) if (unlikely(cond))

#define __unreachable()    __builtin_unreachable()
#define __fallthrough      __attribute__((fallthrough))
#define compiler_barrier() __asm__ volatile("" ::: "memory")

#ifdef __clang__
#    define __annotate(name)           __attribute__((annotate("vl_" #name)))
#    define __annotate_v(name, v)      __attribute__((annotate("vl_" #name "=" #v)))
#    define __unit_count(unit)         __attribute__((annotate("vl_unit_count=" #unit)))
#    define __unit_index(unit, domain) __attribute__((annotate("vl_unit_index=" #unit ":" #domain)))
#    define __flagset(domain)          __attribute__((annotate("vl_flagset=" #domain)))
#else
#    define __annotate(name)
#    define __annotate_v(name, v)
#    define __unit_count(unit)
#    define __unit_index(unit, domain)
#    define __flagset(domain)
#endif

#define __in           __annotate("in")
#define __out          __annotate("out")
#define __out_optional __annotate("out_optional")
#define __inout        __annotate("inout")
#define __buf          __annotate("buf")
#define __nullized         __annotate("nullized")
#define __success_nullized __annotate("success_nullized")

#define __bitwise __annotate("bitwise")
#define __nocast  __annotate("nocast")

#endif  // __VELLUM_COMPILER_H__
