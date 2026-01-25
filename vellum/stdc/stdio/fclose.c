#include <stdio.h>
#include <stdlib.h>

#include <vellum/filesystem.h>

int fclose(FILE *stream)
{
    switch (stream->type) {
    case 1:
        stream->file.file->fs->driver->close(stream->file.file);
        break;
    default:
    }

    free(stream);

    return 0;
}
