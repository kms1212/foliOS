#ifndef __STRING_H__
#define __STRING_H__

#include <stddef.h>

#include <vellum/compiler.h>

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
size_t strcspn(const char *p1, const char *p2);  // not implemented yet
char *strdup(const char *str);
size_t strlen(const char *str);
int strncmp(const char *p1, const char *p2, size_t maxlen);
char *strndup(const char *str, size_t size);
size_t strnlen(const char *str, size_t maxlen);
char *strpbrk(const char *p1, const char *p2);  // not implemented yet
char *strrchr(const char *str, int ch);
size_t strspn(const char *p1, const char *p2);      // not implemented yet
char *strstr(const char *str, const char *substr);  // not implemented yet

const char *strerror(int error);
char *strtok(char *__restrict str, const char *__restrict delim);

#endif  // __STRING_H__
