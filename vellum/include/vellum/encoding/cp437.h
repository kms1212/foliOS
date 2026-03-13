#ifndef __VELLUM_ENCODING_CP437_H__
#define __VELLUM_ENCODING_CP437_H__

#include <stdint.h>
#include <wchar.h>

#include <vellum/status.h>

status_t VlEnc_Utf32ToCp437(wchar_t utf32, uint8_t *cp437out);

#endif  // __VELLUM_ENCODING_CP437_H__
