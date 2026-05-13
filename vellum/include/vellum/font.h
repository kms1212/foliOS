#ifndef __VELLUM_FONT_H__
#define __VELLUM_FONT_H__

#include <wchar.h>

#include <vellum/status.h>

VlStatus VlFont_Use(const char *path);

VlStatus VlFont_GetGlyphDimension(wchar_t codepoint, int *width, int *height);
VlStatus VlFont_GetGlyphData(wchar_t codepoint, uint8_t *buf, size_t size);

#endif  // __VELLUM_FONT_H__
