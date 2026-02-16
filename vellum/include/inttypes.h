#ifndef __INTTYPES_H__
#define __INTTYPES_H__

#include <stdint.h>

#if UINTPTR_MAX == 0xffffffffffffffffULL
#    define PRId8  "d"
#    define PRId16 "d"
#    define PRId32 "d"
#    define PRId64 "ld"
#    define PRIu8  "u"
#    define PRIu16 "u"
#    define PRIu32 "u"
#    define PRIu64 "lu"
#    define PRIx8  "x"
#    define PRIx16 "x"
#    define PRIx32 "x"
#    define PRIx64 "lx"
#    define PRIX8  "X"
#    define PRIX16 "X"
#    define PRIX32 "X"
#    define PRIX64 "lX"

#elif UINTPTR_MAX == 0xffffffffU
#    define PRId8  "d"
#    define PRId16 "d"
#    define PRId32 "ld"
#    define PRId64 "lld"
#    define PRIu8  "u"
#    define PRIu16 "u"
#    define PRIu32 "lu"
#    define PRIu64 "llu"
#    define PRIx8  "x"
#    define PRIx16 "x"
#    define PRIx32 "lx"
#    define PRIx64 "llx"
#    define PRIX8  "X"
#    define PRIX16 "X"
#    define PRIX32 "lX"
#    define PRIX64 "llX"

#endif

#endif  // __INTTYPES_H__
