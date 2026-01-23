#ifndef __WCHAR_H__
#define __WCHAR_H__

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

typedef wchar_t wint_t;

wint_t fgetwc(FILE *stream);
wint_t getwc(FILE *stream);
wint_t getwchar(void);
int fgetws(wchar_t *__restrict wbuf, int count, FILE *__restrict stream);

wint_t fputwc(wchar_t wch, FILE *stream);
wint_t putwc(wchar_t wch, FILE *stream);
wint_t putwchar(wchar_t wch);
int fputws(const wchar_t *__restrict wstr, FILE *__restrict stream);

int wcwidth(wchar_t ucs);
int wcwidth_cjk(wchar_t ucs);

int wcswidth(const wchar_t *pwcs, size_t n);
int wcswidth_cjk(const wchar_t *pwcs, size_t n);

#endif // __WCHAR_H__
