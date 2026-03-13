#ifndef __ENDIAN_H__
#define __ENDIAN_H__

#include <byteswap.h>
#include <stdint.h>

#include <vellum/arch/processor.h>

#define LITTLE_ENDIAN 1
#define BIG_ENDIAN    2

#if defined(__PROCESSOR_BIG_ENDIAN)
#    define BYTE_ORDER BIG_ENDIAN

#    define be16toh(v) (v)
#    define be32toh(v) (v)
#    define be64toh(v) (v)

#    define le16toh(v) bswap_16(v)
#    define le32toh(v) bswap_32(v)
#    define le64toh(v) bswap_64(v)

#    define htobe16(v) (v)
#    define htobe32(v) (v)
#    define htobe64(v) (v)

#    define htole16(v) bswap_16(v)
#    define htole32(v) bswap_32(v)
#    define htole64(v) bswap_64(v)

#elif defined(__PROCESSOR_LITTLE_ENDIAN)
#    define BYTE_ORDER LITTLE_ENDIAN

#    define be16toh(v) bswap_16(v)
#    define be32toh(v) bswap_32(v)
#    define be64toh(v) bswap_64(v)

#    define le16toh(v) (v)
#    define le32toh(v) (v)
#    define le64toh(v) (v)

#    define htobe16(v) bswap_16(v)
#    define htobe32(v) bswap_32(v)
#    define htobe64(v) bswap_64(v)

#    define htole16(v) (v)
#    define htole32(v) (v)
#    define htole64(v) (v)

#else
#    error Processor endianness is unknown

#endif

#endif  // __ENDIAN_H__
