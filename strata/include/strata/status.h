#ifndef __STRATA_STATUS_H__
#define __STRATA_STATUS_H__

#include <stdint.h>

#include <strata/compiler.h>

typedef uint32_t StStatus __nocast;

#define STATUS_FAILURE_MASK  ((StStatus)0x80000000)
#define STATUS_SEVERITY_MASK ((StStatus)0x40000000)
#define STATUS_AREA_MASK     ((StStatus)0x3FFF0000)
#define STATUS_CODE_MASK     ((StStatus)0x0000FFFF)

#define CHECK_SUCCESS(status) (!((status) & (StStatus)0x80000000))
#define CHECK_FAILURE(status) (!!((status) & (StStatus)0x80000000))

#define CHECK_NORMAL_FAILURE(status) (CHECK_FAILURE(status) && (!((status) & (StStatus)0x40000000)))
#define CHECK_CRITICAL_FAILURE(status)                                                             \
    (CHECK_FAILURE(status) && (!!((status) & (StStatus)0x40000000)))

#define STATUS_SUCCESS               ((StStatus)0x00000000)
#define STATUS_DEPRECATED            ((StStatus)0x00000001)
#define STATUS_NO_EVENT              ((StStatus)0x00000002)
#define STATUS_MUTEX_LOCKED          ((StStatus)0x00000003)
#define STATUS_INVALID_UTF8_SEQUENCE ((StStatus)0x00000004)

#define STATUS_UNKNOWN_ERROR            ((StStatus)0x80000000)
#define STATUS_INVALID_RESOURCE         ((StStatus)0x80000001)
#define STATUS_UNEXPECTED_RESULT        ((StStatus)0x80000002)
#define STATUS_INVALID_SIGNATURE        ((StStatus)0x80000003)
#define STATUS_INVALID_VALUE            ((StStatus)0x80000004)
#define STATUS_UNSUPPORTED              ((StStatus)0x80000005)
#define STATUS_ENTRY_NOT_FOUND          ((StStatus)0x80000006)
#define STATUS_UNIMPLEMENTED            ((StStatus)0x80000007)
#define STATUS_HARDWARE_NOT_FOUND       ((StStatus)0x80000008)
#define STATUS_HARDWARE_FAILED          ((StStatus)0x80000009)
#define STATUS_IO_TIMEOUT               ((StStatus)0x8000000A)
#define STATUS_BUFFER_UNDERFLOW         ((StStatus)0x8000000B)
#define STATUS_CONFLICTING_STATE        ((StStatus)0x8000000C)
#define STATUS_END_OF_LIST              ((StStatus)0x8000000D)
#define STATUS_WRONG_ELEMENT_TYPE       ((StStatus)0x8000000E)
#define STATUS_DUPLICATE_ENTRY          ((StStatus)0x8000000F)
#define STATUS_SYNTAX_ERROR             ((StStatus)0x80000010)
#define STATUS_END_OF_FILE              ((StStatus)0x80000011)
#define STATUS_INVALID_FORMAT           ((StStatus)0x80000012)
#define STATUS_SIZE_CHECK_FAILURE       ((StStatus)0x80000013)
#define STATUS_BOOT_DEVICE_INACCESSIBLE ((StStatus)0x80000014)
#define STATUS_INVALID_SYNTAX           ((StStatus)0x80000015)
#define STATUS_BUFFER_TOO_SMALL         ((StStatus)0x80000016)
#define STATUS_PAGE_NOT_PRESENT         ((StStatus)0x80000017)
#define STATUS_INSUFFICIENT_MEMORY      ((StStatus)0x80000018)
#define STATUS_FEATURE_DISABLED         ((StStatus)0x80000019)
#define STATUS_THREAD_NOT_FINISHED      ((StStatus)0x8000001A)
#define STATUS_INVALID_THREAD           ((StStatus)0x8000001B)
#define STATUS_NOT_ALLOCATED            ((StStatus)0x8000001C)

#define STATUS_UNKNOWN_CRITICAL_ERROR  ((StStatus)0xC0000000)
#define STATUS_ASSERTION_FAILED        ((StStatus)0xC0000001)
#define STATUS_FS_INCONSISTENT         ((StStatus)0xC0000002)
#define STATUS_SYSTEM_CORRUPTED        ((StStatus)0xC0000003)
#define STATUS_PHYSICAL_MEMORY_TOO_BIG ((StStatus)0xC0000004)

#endif  // __STRATA_STATUS_H__
