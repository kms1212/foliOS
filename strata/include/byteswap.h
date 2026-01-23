#ifndef __BYTESWAP_H__
#define __BYTESWAP_H__

#include <stdint.h>

uint16_t bswap_16(uint16_t val);
uint32_t bswap_32(uint32_t val);
uint64_t bswap_64(uint64_t val);

#if __has_builtin(__builtin_bswap16)
#   define __HAVE_BUILTIN_BSWAP16
#   define bswap_16(v) __builtin_bswap16(v);

#endif


#if __has_builtin(__builtin_bswap32)
#   define __HAVE_BUILTIN_BSWAP32
#   define bswap_32(v) __builtin_bswap32(v);

#endif


#if __has_builtin(__builtin_bswap64)
#   define __HAVE_BUILTIN_BSWAP64
#   define bswap_64(v) __builtin_bswap64(v);

#endif

#endif // __BYTESWAP_H__
