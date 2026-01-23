#ifndef __VELLUM_PATH_H__
#define __VELLUM_PATH_H__

#include <stddef.h>
#include <stdio.h>
#include <limits.h>

struct path_iterator {
    const char *path;
    const char *cursor;
    char element[FILENAME_MAX];
    int has_separator;
};

void path_iter_init(struct path_iterator *__restrict it, const char *__restrict path);
int path_iter_next(struct path_iterator *it);

char *path_join(char *__restrict dest, size_t len, const char *__restrict src);
char *path_normalize(char *__restrict dest, size_t len, const char *__restrict src);

char *path_get_fsname(char *__restrict buf, size_t len, const char *__restrict path);
char *path_get_dirname(char *__restrict buf, size_t len, const char *__restrict path);
char *path_get_basename(char *__restrict buf, size_t len, const char *__restrict path);
char *path_get_stem(char *__restrict buf, size_t len, const char *__restrict path);
char *path_get_extension(char *__restrict buf, size_t len, const char *__restrict path);

int path_is_absolute(const char *path);
int path_compare(const char *__restrict path1, const char *__restrict path2, int case_sensitive);

#endif // __VELLUM_PATH_H__
