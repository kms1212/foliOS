#ifndef __INTTYPES_H__
#define __INTTYPES_H__

#include <stdint.h>

#if __SIZEOF_INT__ == 4
#    define STRATA_PRI32_PREFIX ""
#elif __SIZEOF_LONG__ == 4
#    define STRATA_PRI32_PREFIX "l"
#else
#    error "unsupported int32_t printf format"
#endif

#if __SIZEOF_LONG__ == 8
#    define STRATA_PRI64_PREFIX "l"
#elif __SIZEOF_LONG_LONG__ == 8
#    define STRATA_PRI64_PREFIX "ll"
#else
#    error "unsupported int64_t printf format"
#endif

#if __SIZEOF_POINTER__ == __SIZEOF_LONG__
#    define STRATA_PRIPTR_PREFIX "l"
#elif __SIZEOF_POINTER__ == __SIZEOF_INT__
#    define STRATA_PRIPTR_PREFIX ""
#elif __SIZEOF_POINTER__ == __SIZEOF_LONG_LONG__
#    define STRATA_PRIPTR_PREFIX "ll"
#else
#    error "unsupported intptr_t printf format"
#endif

#define PRId8  "d"
#define PRId16 "d"
#define PRId32 STRATA_PRI32_PREFIX "d"
#define PRId64 STRATA_PRI64_PREFIX "d"
#define PRIu8  "u"
#define PRIu16 "u"
#define PRIu32 STRATA_PRI32_PREFIX "u"
#define PRIu64 STRATA_PRI64_PREFIX "u"
#define PRIx8  "x"
#define PRIx16 "x"
#define PRIx32 STRATA_PRI32_PREFIX "x"
#define PRIx64 STRATA_PRI64_PREFIX "x"
#define PRIX8  "X"
#define PRIX16 "X"
#define PRIX32 STRATA_PRI32_PREFIX "X"
#define PRIX64 STRATA_PRI64_PREFIX "X"

#define PRIdPTR STRATA_PRIPTR_PREFIX "d"
#define PRIuPTR STRATA_PRIPTR_PREFIX "u"
#define PRIxPTR STRATA_PRIPTR_PREFIX "x"
#define PRIXPTR STRATA_PRIPTR_PREFIX "X"

#endif  // __INTTYPES_H__
