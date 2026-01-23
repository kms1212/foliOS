#ifndef __STRATA_TYPES_H__
#define __STRATA_TYPES_H__

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include <strata/compiler.h>

typedef uintptr_t St_PhysFrame __nocast;
typedef uintptr_t St_VirtPage __nocast;

typedef size_t St_PageCount __nocast;

typedef uint16_t uint_be16_t __bitwise;
typedef uint32_t uint_be32_t __bitwise;
typedef uint64_t uint_be64_t __bitwise;

typedef uint16_t uint_le16_t __bitwise;
typedef uint32_t uint_le32_t __bitwise;
typedef uint64_t uint_le64_t __bitwise;

#endif // __STRATA_TYPES_H__
