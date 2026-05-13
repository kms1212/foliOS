#include <strata/gnt/path.h>

#include <assert.h>
#include <string.h>

#include <strata/compiler.h>
#include <strata/utf.h>

void StGntPath_Begin(struct StGnt_PathCursor *cursor __out, const St_Utf32Char *path __in)
{
    assert(cursor);

    cursor->path = path;
    cursor->token = NULL;
    cursor->token_len = 0;
}

int StGntPath_Next(struct StGnt_PathCursor *cursor __inout)
{
    assert(cursor);

    const St_Utf32Char *path;

    path = cursor->path;
    while (*path == '/') {
        path++;
    }
    if (*path == '\0') return 1;

    cursor->token = path;
    cursor->token_len = 0;

    while (*path != '/' && *path != '\0') {
        cursor->token_len++;
        path++;
    }

    cursor->path = path;

    return 0;
}

const St_Utf32Char *StGntPath_Remaining(const struct StGnt_PathCursor *cursor __in)
{
    return cursor->path;
}

int StGntPath_IsToken(
    const struct StGnt_PathCursor *cursor __in, const St_Utf32Char *name __in, size_t name_len __in
)
{
    if (!cursor || !cursor->token) return 0;
    if (cursor->token_len != name_len) return 0;

    return memcmp(cursor->token, name, name_len * sizeof(*name)) == 0;
}

int StGntPath_IsDot(const struct StGnt_PathCursor *cursor __in)
{
    return StGntPath_IsToken(cursor, U".", 1);
}

int StGntPath_IsDotDot(const struct StGnt_PathCursor *cursor __in)
{
    return StGntPath_IsToken(cursor, U"..", 2);
}
