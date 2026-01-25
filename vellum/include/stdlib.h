#ifndef __STDLIB_H__
#define __STDLIB_H__

#include <stddef.h>

#include <liballoc.h>

#include <vellum/compiler.h>

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1

#define RAND_MAX 0x7FFF

__noreturn static inline void exit(int i)
{
    for (;;) {
    }
}

double atof(const char *str);

int atoi(const char *str);

long atol(const char *str);

long long atoll(const char *str);

float strtof(const char *__restrict str, char **__restrict endptr);

double strtod(const char *__restrict str, char **__restrict endptr);

long double strtold(const char *__restrict str, char **__restrict endptr);

long strtol(const char *__restrict str, char **__restrict endptr, int base);

long long strtoll(const char *__restrict str, char **__restrict endptr, int base);

unsigned long strtoul(const char *__restrict str, char **__restrict endptr, int base);

unsigned long long strtoull(const char *__restrict str, char **__restrict endptr, int base);

void qsort(void *base, size_t num, size_t size, int (*cmp)(const void *, const void *));

void *bsearch(
    const void *key,
    const void *base,
    size_t num,
    size_t size,
    int (*cmp)(const void *, const void *)
);

typedef struct {
    int quot;
    int rem;
} div_t;

typedef struct {
    long quot;
    long rem;
} ldiv_t;

typedef struct {
    long long quot;
    long long rem;
} lldiv_t;

int abs(int n);

div_t div(int numer, int denom);

long labs(long n);

ldiv_t ldiv(long numer, long denom);

long long llabs(long long n);

lldiv_t lldiv(long long numer, long long denom);

int rand(void);

void srand(unsigned int seed);

#endif  // __STDLIB_H__
