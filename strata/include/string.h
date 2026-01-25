#ifndef __STRING_H__
#define __STRING_H__

#include <stddef.h>

#include <strata/compiler.h>

void *memcpy(void *__restrict dest, const void *__restrict src, size_t len);
void *memmove(void *dest, const void *src, size_t len);
void *mempcpy(void *__restrict dest, const void *__restrict src, size_t len);
void *memset(void *dest, int c, size_t count);
char *strcat(char *__restrict dest, const char *__restrict src);
char *strcpy(char *__restrict dest, const char *__restrict src);
char *strncat(char *__restrict dest, const char *__restrict src, size_t maxlen);
char *strncpy(char *__restrict dest, const char *__restrict src, size_t maxlen);
char *stpcpy(char *__restrict dest, const char *__restrict src);
char *stpncpy(char *__restrict dest, const char *__restrict src, size_t maxlen);

void *memchr(const void *ptr, int value, size_t len);  // not implemented yet
int memcmp(const void *p1, const void *p2, size_t len);
char *strchr(const char *str, int ch);
int strcmp(const char *p1, const char *p2);
char *strerror(int errnum);
size_t strcspn(const char *p1, const char *p2);  // not implemented yet
size_t strlen(const char *str);
int strncmp(const char *p1, const char *p2, size_t maxlen);
size_t strnlen(const char *str, size_t maxlen);
char *strpbrk(const char *p1, const char *p2);  // not implemented yet
char *strrchr(const char *str, int ch);
size_t strspn(const char *p1, const char *p2);      // not implemented yet
char *strstr(const char *str, const char *substr);  // not implemented yet

#if __has_builtin(__builtin_memcpy)
#    define __HAVE_BUILTIN_MEMCPY

#endif

#if __has_builtin(__builtin_memmove)
#    define __HAVE_BUILTIN_MEMMOVE

#endif

#if __has_builtin(__builtin_mempcpy)
#    define __HAVE_BUILTIN_MEMPCPY

#endif

#if __has_builtin(__builtin_memset)
#    define __HAVE_BUILTIN_MEMSET

#endif

#if __has_builtin(__builtin_strcat)
#    define __HAVE_BUILTIN_STRCAT

#endif

#if __has_builtin(__builtin_strcpy)
#    define __HAVE_BUILTIN_STRCPY

#endif

#if __has_builtin(__builtin_strncat)
#    define __HAVE_BUILTIN_STRNCAT

#endif

#if __has_builtin(__builtin_strncpy)
#    define __HAVE_BUILTIN_STRNCPY

#endif

#if __has_builtin(__builtin_stpncpy)
#    define __HAVE_BUILTIN_STPNCPY

#endif

#if __has_builtin(__builtin_stpcpy)
#    define __HAVE_BUILTIN_STPCPY

#endif

#if __has_builtin(__builtin_memchr)
#    define __HAVE_BUILTIN_MEMCHR
#    define memchr(p1, ...) __builtin_memchr(p1, __VA_ARGS__)

#endif

#if __has_builtin(__builtin_memcmp)
#    define __HAVE_BUILTIN_MEMCMP
#    define memcmp(p1, ...) __builtin_memcmp(p1, __VA_ARGS__)

#endif

#if __has_builtin(__builtin_strchr)
#    define __HAVE_BUILTIN_STRCHR
#    define strchr(p1, ...) __builtin_strchr(p1, __VA_ARGS__)

#endif

#if __has_builtin(__builtin_strcmp)
#    define __HAVE_BUILTIN_STRCMP
#    define strcmp(p1, ...) __builtin_strcmp(p1, __VA_ARGS__)

#endif

#if __has_builtin(__builtin_strcspn)
#    define __HAVE_BUILTIN_STRCSPN
#    define strcspn(p1, ...) __builtin_strcspn(p1, __VA_ARGS__)

#endif

#if __has_builtin(__builtin_strlen)
#    define __HAVE_BUILTIN_STRLEN
#    define strlen(str) __builtin_strlen(str)

#endif

#if __has_builtin(__builtin_strncmp)
#    define __HAVE_BUILTIN_STRNCMP
#    define strncmp(p1, ...) __builtin_strncmp(p1, __VA_ARGS__)

#endif

#if __has_builtin(__builtin_strnlen)
#    define __HAVE_BUILTIN_STRNLEN
#    define strnlen(p1, ...) __builtin_strnlen(p1, __VA_ARGS__)

#endif

#if __has_builtin(__builtin_strpbrk)
#    define __HAVE_BUILTIN_STRPBRK
#    define strpbrk(p1, ...) __builtin_strpbrk(p1, __VA_ARGS__)

#endif

#if __has_builtin(__builtin_strrchr)
#    define __HAVE_BUILTIN_STRRCHR
#    define strrchr(p1, ...) __builtin_strrchr(p1, __VA_ARGS__)

#endif

#if __has_builtin(__builtin_strspn)
#    define __HAVE_BUILTIN_STRPBRK
#    define strspn(p1, ...) __builtin_strspn(p1, __VA_ARGS__)

#endif

#if __has_builtin(__builtin_strstr)
#    define __HAVE_BUILTIN_STRSTR
#    define strstr(p1, ...) __builtin_strstr(p1, __VA_ARGS__)

#endif

#endif  // __STRING_H__
