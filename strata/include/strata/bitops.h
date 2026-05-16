#ifndef __STRATA_BITOPS_H__
#define __STRATA_BITOPS_H__

#include <assert.h>
#include <stdint.h>

#include <strata/compiler.h>

static __always_inline int St_CountTrailingZeros32(uint32_t value __in)
{
    assert(value != 0);
    return __builtin_ctz((unsigned int)value);
}

static __always_inline int St_CountTrailingZeros64(uint64_t value __in)
{
    assert(value != 0);
    return __builtin_ctzll((unsigned long long)value);
}

static __always_inline int St_CountLeadingZeros32(uint32_t value __in)
{
    assert(value != 0);
    return __builtin_clz((unsigned int)value);
}

static __always_inline int St_CountLeadingZeros64(uint64_t value __in)
{
    assert(value != 0);
    return __builtin_clzll((unsigned long long)value);
}

static __always_inline int St_CountSetBits32(uint32_t value __in)
{
    return __builtin_popcount((unsigned int)value);
}

static __always_inline int St_CountSetBits64(uint64_t value __in)
{
    return __builtin_popcountll((unsigned long long)value);
}

#endif  // __STRATA_BITOPS_H__
