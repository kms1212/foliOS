#ifndef __STRATA_COMPILER_H__
#define __STRATA_COMPILER_H__

#define __in
#define __in_optional
#define __out               [static 1]
#define __out_optional      []
#define __inout             [static 1]

#define __always_inline     inline __attribute__((always_inline))

#ifndef __unused
#   define __unused            __attribute__((unused))

#endif

#define __noreturn          __attribute__((noreturn))
#define __naked             __attribute__((naked))
#define __packed            __attribute__((packed))
#define __format_printf(fmt, chk) __attribute__((format(printf, fmt, chk)))
#define __aligned(n)        __attribute__((aligned(n)))
#define __constructor       __attribute__((constructor))
#define __destructor        __attribute__((destructor))
#define __section(s)        __attribute__((section(#s)))

#ifndef __weak
#   define __weak              __attribute__((weak))

#endif

#define __hot               __attribute__((hot))

#ifndef __cold
#   define __cold              __attribute__((cold))

#endif

#ifndef __pure
#   define __pure              __attribute__((pure))

#endif

#ifndef __deprecated 
#   define __deprecated        __attribute__((deprecated))

#endif

#define __externally_visible __attribute__((externally_visible))
#define __sentinel          __attribute__((sentinel))
#define __warn_unused_result __attribute__((warn_unused_result))

#ifdef __CHECKER__
#   define __kernel         __attribute__((address_space(0)))
#   define __percpu         __attribute__((noderef, address_space(1)))
#   define __iomem          __attribute__((noderef, address_space(2)))
#   define __module         __attribute__((noderef, address_space(3)))
#   define __user           __attribute__((noderef, address_space(4)))

__always_inline void __chk_user_ptr(const volatile void __user *ptr) {}
__always_inline void __chk_module_ptr(const volatile void __module *ptr) {}
__always_inline void __chk_iomem_ptr(const volatile void __iomem *ptr) {}

#   define __must_hold(x)   __attribute__((context(x, 1, 1)))
#   define __acquires(x)    __attribute__((context(x, 0, 1)))
#   define __cond_acquires(x) __attribute__((context(x, 0, -1)))
#   define __releases(x)    __attribute__((context(x, 1, 0)))
#   define __acquire(x)     __context__(x, 1)
#   define __release(x)     __context__(x, -1)
#   define __cond_lock(x, c) ((c) ? ({ __acquire(x); 1; }) : 0)

#   define __force          __attribute__((force))
#   define __bitwise        __attribute__((bitwise))
#   define __nocast         __attribute__((nocast))
#   define __safe           __attribute__((safe))
#   define __private        __attribute__((noderef))
#   define ACCESS_PRIVATE(p, member) (*((typeof((p)->member) __force *) &(p)->member))

#else
#   define __kernel
#   define __user
#   define __module
#   define __iomem
#   define __percpu

#   define __chk_user_ptr(x) ((void)0)
#   define __chk_module_ptr(x) ((void)0)
#   define __chk_iomem_ptr(x) ((void)0)

#   define __must_hold(x)
#   define __acquires(x)
#   define __cond_acquires(x)
#   define __releases(x)
#   define __acquire(x)     ((void)0)
#   define __release(x)     ((void)0)
#   define __cond_lock(x, c) (c)

#   define __force
#   define __bitwise
#   define __nocast
#   define __safe
#   define __private
#   define ACCESS_PRIVATE(p, member) ((p)->member)

#endif

#endif // __STRATA_COMPILER_H__
