#ifndef __CRC32_H__
#define __CRC32_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

uint32_t crc32(const uint8_t *message, unsigned int len);

#ifdef __cplusplus
};
#endif

#endif // __CRC32_H__
