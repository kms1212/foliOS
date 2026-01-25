#ifndef __ASSERT_H__
#define __ASSERT_H__

#ifdef NDEBUG
#    define assert(e) ((void)0)

#else

#    include <vellum/compiler.h>

extern void __assert_fail(
    const char *assertion, const char *file, unsigned int line, const char *func
);

#    define assert(e) ((void)((e) || (__assert_fail(#e, __FILE__, __LINE__, __ASSERT_FUNCTION), 0)))

#    if (!defined(__GNUC__) || __GNUC__ < 2 || __GNUC_MINOR__ < (defined(__cplusplus) ? 6 : 4))
#        define __ASSERT_FUNCTION ((__const char *)0)

#    else
#        define __ASSERT_FUNCTION __PRETTY_FUNCTION__

#    endif

#endif

#endif  // __ASSERT_H__
