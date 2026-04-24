#ifndef __STRATA_GNT_PATH_H__
#define __STRATA_GNT_PATH_H__

#include <stddef.h>

#include <strata/compiler.h>
#include <strata/utf.h>

struct StGnt_PathCursor {
    const St_Utf32Char *path;
    const St_Utf32Char *token;
    size_t token_len;
};

void StGntPath_Begin(struct StGnt_PathCursor *cursor __out, const St_Utf32Char *path __in);
int StGntPath_Next(struct StGnt_PathCursor *cursor __inout);
const St_Utf32Char *StGntPath_Remaining(const struct StGnt_PathCursor *cursor __in);
int StGntPath_IsToken(
    const struct StGnt_PathCursor *cursor __in, const St_Utf32Char *name __in, size_t name_len __in
);
int StGntPath_IsDot(const struct StGnt_PathCursor *cursor __in);
int StGntPath_IsDotDot(const struct StGnt_PathCursor *cursor __in);

#endif  // __STRATA_GNT_PATH_H__
