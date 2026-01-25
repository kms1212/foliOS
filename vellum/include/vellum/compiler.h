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

#endif  // __VELLUM_COMPILER_H__
