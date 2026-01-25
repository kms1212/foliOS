#ifndef __VELLUM_DEBUG_H__
#define __VELLUM_DEBUG_H__

#include <stdint.h>
#include <stdio.h>

#include <vellum/compiler.h>
#include <vellum/status.h>

void stacktrace(const void *base);

void hexdump(FILE *fp, const void *data, long len, uint32_t offset);

#endif  // __VELLUM_DEBUG_H__
