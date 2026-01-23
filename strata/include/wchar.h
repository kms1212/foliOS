#ifndef __WCHAR_H__
#define __WCHAR_H__

#include <stddef.h>
#include <stdio.h>

typedef wchar_t wint_t;

int wcwidth(wchar_t ucs);
int wcwidth_cjk(wchar_t ucs);

int wcswidth(const wchar_t *pwcs, size_t n);
int wcswidth_cjk(const wchar_t *pwcs, size_t n);

#endif // __WCHAR_H__
