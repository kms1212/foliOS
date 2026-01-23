#ifndef __STRINGS_H__
#define __STRINGS_H__

#include <stddef.h>

int ffs(int val);
int ffsl(long val);
int ffsll(long long val);

int strncasecmp(const char *p1, const char *p2, size_t maxlen);
int strcasecmp(const char *p1, const char *p2);

#endif // __STRINGS_H__