#ifndef __VELLUM_PATH_H__
#define __VELLUM_PATH_H__

#include <limits.h>
#include <stddef.h>
#include <stdio.h>

struct path_iterator {
    const char *path;
    const char *cursor;
    char element[FILENAME_MAX];
    int has_separator;
};

void VlPath_InitIter(struct path_iterator *__restrict it, const char *__restrict path);
int VlPath_AdvanceIter(struct path_iterator *it);

char *VlPath_Join(char *__restrict dest, size_t len, const char *__restrict src);
char *VlPath_Normalize(char *__restrict dest, size_t len, const char *__restrict src);

char *VlPath_GetFsname(char *__restrict buf, size_t len, const char *__restrict path);
char *VlPath_GetDirname(char *__restrict buf, size_t len, const char *__restrict path);
char *VlPath_GetBasename(char *__restrict buf, size_t len, const char *__restrict path);
char *VlPath_GetStem(char *__restrict buf, size_t len, const char *__restrict path);
char *VlPath_GetExtension(char *__restrict buf, size_t len, const char *__restrict path);

int VlPath_IsAbsolute(const char *path);
int VlPath_Compare(const char *__restrict path1, const char *__restrict path2, int case_sensitive);

#endif  // __VELLUM_PATH_H__
