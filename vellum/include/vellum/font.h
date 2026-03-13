#ifndef __VELLUM_FONT_H__
#define __VELLUM_FONT_H__

#include <wchar.h>

#include <vellum/status.h>

status_t VlFont_Use(const char *path);

status_t VlFont_GetGlyphDimension(wchar_t codepoint, int *width, int *height);
status_t VlFont_GetGlyphData(wchar_t codepoint, uint8_t *buf, size_t size);

#endif  // __VELLUM_FONT_H__
