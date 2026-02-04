#ifndef _LIBALLOC_H
#define _LIBALLOC_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void *la_malloc(size_t);           //< The standard function.
void *la_realloc(void *, size_t);  //< The standard function.
void *la_calloc(size_t, size_t);   //< The standard function.
void la_free(void *);              //< The standard function.

#ifdef __cplusplus
}
#endif

#endif
