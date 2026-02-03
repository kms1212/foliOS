#ifndef __STRATA_TYPES_H__
#define __STRATA_TYPES_H__

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <strata/compiler.h>

typedef ptrdiff_t ssize_t;

typedef uint16_t uint_be16_t __bitwise;
typedef uint32_t uint_be32_t __bitwise;
typedef uint64_t uint_be64_t __bitwise;

typedef uint16_t uint_le16_t __bitwise;
typedef uint32_t uint_le32_t __bitwise;
typedef uint64_t uint_le64_t __bitwise;

#endif  // __STRATA_TYPES_H__
