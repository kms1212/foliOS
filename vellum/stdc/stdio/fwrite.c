#include <stdio.h>

#include <vellum/interface/char.h>

size_t fwrite(const void *__restrict ptr, size_t size, size_t count, FILE *__restrict stream)
{
    status_t status;
    size_t write_count = 0;

    while (write_count < count) {
        switch (stream->type) {
        case 2:
            status = stream->dev.charif->write(stream->dev.dev, ptr, size, NULL);
            if (!CHECK_SUCCESS(status)) goto end;
            break;
        case 3:
            return stream->cookie.io_funcs.write(stream->cookie.cookie, ptr, size);
        default:
            return -1;
        }
        write_count++;
        ptr = (uint8_t *)ptr + size;
    }
end:

    return write_count;
}
