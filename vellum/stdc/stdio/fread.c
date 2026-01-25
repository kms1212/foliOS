#include <stdio.h>

#include <vellum/filesystem.h>
#include <vellum/interface/char.h>

size_t fread(void *__restrict ptr, size_t size, size_t count, FILE *__restrict stream)
{
    status_t status;
    size_t read_count = 0;
    ssize_t cookie_result;

    while (read_count < count) {
        switch (stream->type) {
        case 1:
            status = stream->file.file->fs->driver->read(stream->file.file, ptr, size, NULL);
            if (!CHECK_SUCCESS(status)) goto end;
            break;
        case 2:
            status = stream->dev.charif->read(stream->dev.dev, ptr, size, NULL);
            if (!CHECK_SUCCESS(status)) goto end;
            break;
        case 3:
            cookie_result = stream->cookie.io_funcs.read(stream->cookie.cookie, ptr, size);
            if (cookie_result != count) goto end;
        default:
            return -1;
        }
        read_count++;
        ptr = (uint8_t *)ptr + size;
    }
end:

    return read_count;
}
