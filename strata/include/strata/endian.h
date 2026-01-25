#ifndef __STRATA_ENDIAN_H__
#define __STRATA_ENDIAN_H__

#include <stdint.h>

#include <strata/arch/processor.h>

#include <strata/types.h>

__always_inline uint16_t St_SwapEndian16(uint16_t v)
{
    return __builtin_bswap16(v);
}

__always_inline uint32_t St_SwapEndian32(uint32_t v)
{
    return __builtin_bswap32(v);
}

__always_inline uint64_t St_SwapEndian64(uint64_t v)
{
    return __builtin_bswap64(v);
}

#if defined(__PROCESSOR_BIG_ENDIAN)

__always_inline uint16_t St_BeToHost16(uint_be16_t v)
{
    return (uint16_t)v;
}

__always_inline uint32_t St_BeToHost32(uint_be32_t v)
{
    return (uint32_t)v;
}

__always_inline uint64_t St_BeToHost64(uint_be64_t v)
{
    return (uint64_t)v;
}

__always_inline uint16_t St_LeToHost16(uint_le16_t v)
{
    return St_SwapEndian16((uint16_t)v);
}

__always_inline uint32_t St_LeToHost32(uint_le32_t v)
{
    return St_SwapEndian32((uint32_t)v);
}

__always_inline uint64_t St_LeToHost64(uint_le64_t v)
{
    return St_SwapEndian64((uint64_t)v);
}

__always_inline uint_be16_t St_HostToBe16(uint16_t v)
{
    return (uint_be16_t)St_SwapEndian16(v);
}

__always_inline uint_be32_t St_HostToBe32(uint32_t v)
{
    return (uint_be32_t)St_SwapEndian32(v);
}

__always_inline uint_be64_t St_HostToBe64(uint64_t v)
{
    return (uint_be64_t)St_SwapEndian64(v);
}

__always_inline uint_le16_t St_HostToLe16(uint16_t v)
{
    return (uint_le16_t)St_SwapEndian16(v);
}

__always_inline uint_le32_t St_HostToLe32(uint32_t v)
{
    return (uint_le32_t)St_SwapEndian32(v);
}

__always_inline uint_le64_t St_HostToLe64(uint64_t v)
{
    return (uint_le64_t)St_SwapEndian64(v);
}

#elif defined(__PROCESSOR_LITTLE_ENDIAN)

__always_inline uint16_t St_BeToHost16(uint_be16_t v)
{
    return St_SwapEndian16((uint16_t)v);
}

__always_inline uint32_t St_BeToHost32(uint_be32_t v)
{
    return St_SwapEndian32((uint32_t)v);
}

__always_inline uint64_t St_BeToHost64(uint_be64_t v)
{
    return St_SwapEndian64((uint64_t)v);
}

__always_inline uint16_t St_LeToHost16(uint_le16_t v)
{
    return (uint16_t)v;
}

__always_inline uint32_t St_LeToHost32(uint_le32_t v)
{
    return (uint32_t)v;
}

__always_inline uint64_t St_LeToHost64(uint_le64_t v)
{
    return (uint64_t)v;
}

__always_inline uint_be16_t St_HostToBe16(uint16_t v)
{
    return (uint_be16_t)St_SwapEndian16(v);
}

__always_inline uint_be32_t St_HostToBe32(uint32_t v)
{
    return (uint_be32_t)St_SwapEndian32(v);
}

__always_inline uint_be64_t St_HostToBe64(uint64_t v)
{
    return (uint_be64_t)St_SwapEndian64(v);
}

__always_inline uint_le16_t St_HostToLe16(uint16_t v)
{
    return (uint_le16_t)St_SwapEndian16(v);
}

__always_inline uint_le32_t St_HostToLe32(uint32_t v)
{
    return (uint_le32_t)St_SwapEndian32(v);
}

__always_inline uint_le64_t St_HostToLe64(uint64_t v)
{
    return (uint_le64_t)St_SwapEndian64(v);
}

#else
#   error Processor endianness is unknown

#endif

#endif // __STRATA_ENDIAN_H__
