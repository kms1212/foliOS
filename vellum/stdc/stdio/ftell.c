#include <stdio.h>

#include <vellum/filesystem.h>
#include <vellum/status.h>
#include <vellum/types.h>

#include "internal.h"

long ftell(FILE *stream)
{
    VlStatus status;
    off_t offset;

    switch (stream->type) {
    case 1:
        status = stream->file.file->fs->driver->tell(stream->file.file, &offset);
        if (!CHECK_SUCCESS(status)) return -1;
        return offset;
    default:
        return -1;
    }
}
