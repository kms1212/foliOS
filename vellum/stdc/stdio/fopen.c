#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <vellum/path.h>
#include <vellum/filesystem.h>

FILE *fopen(const char *__restrict path, const char *__restrict mode)
{
    status_t status;
    struct path_iterator pathit;

    path_iter_init(&pathit, path);

    if (!pathit.element[0]) {
        return NULL;
    }

    struct filesystem *fs;
    status = filesystem_find(pathit.element, &fs);
    if (!CHECK_SUCCESS(status)) return NULL;

    struct fs_directory *dir;
    status = fs->driver->open_root_directory(fs, &dir);
    if (!CHECK_SUCCESS(status)) return NULL;

    while (!path_iter_next(&pathit)) {
        if (pathit.element[0] == '\0') {
            continue;
        }

        if (strcmp(".", pathit.element) == 0) {
            continue;
        }

        struct fs_directory *newdir;
        status = dir->fs->driver->open_directory(dir, pathit.element, &newdir);
        if (!CHECK_SUCCESS(status)) return NULL;

        dir->fs->driver->close_directory(dir);
        dir = newdir;
    }

    if (!pathit.element[0]) {
        return NULL;
    }
    
    struct fs_file *file;
    status = fs->driver->open(dir, pathit.element, &file);
    if (!CHECK_SUCCESS(status)) return NULL;

    dir->fs->driver->close_directory(dir);

    FILE *stream = malloc(sizeof(*stream));
    if (!stream) return NULL;

    stream->type = 1;
    stream->file.fs = fs;
    stream->file.file = file;

    return stream;
}

