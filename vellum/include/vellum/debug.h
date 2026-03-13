#ifndef __VELLUM_DEBUG_H__
#define __VELLUM_DEBUG_H__

#include <stdint.h>
#include <stdio.h>

#include <vellum/compiler.h>
#include <vellum/status.h>

void VlDbg_Stacktrace(const void *base);

void VlDbg_Hexdump(FILE *fp, const void *data, long len, uint32_t offset);

#endif  // __VELLUM_DEBUG_H__
