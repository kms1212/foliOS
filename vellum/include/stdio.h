#ifndef __STDIO_H__
#define __STDIO_H__

#include <limits.h>
#include <stdarg.h>
#include <stddef.h>

#include <vellum/compiler.h>
#include <vellum/types.h>

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

#define EOF -1

typedef ssize_t (*cookie_read_function_t)(void *cookie, char *buf, size_t count);
typedef ssize_t (*cookie_write_function_t)(void *cookie, const char *buf, size_t count);
typedef int (*cookie_seek_function_t)(void *cookie, off64_t *pos, int whence);
typedef int (*cookie_close_function_t)(void *cookie);

struct cookie_io_functions {
    cookie_read_function_t read;
    cookie_write_function_t write;
    cookie_seek_function_t seek;
    cookie_close_function_t close;
};

typedef struct cookie_io_functions cookie_io_functions_t;

struct _iobuf {
    int error;
    int type;

    union {
        struct {
            struct filesystem *fs;
            struct fs_file *file;
        } file;

        struct {
            struct device *dev;
            const struct char_interface *charif;
        } dev;

        struct {
            void *cookie;
            cookie_io_functions_t io_funcs;
        } cookie;
    };
};

typedef struct _iobuf FILE;

extern FILE *stdin, *stdout, *stderr, *stddbg;

__format_printf(3, 4) int cprintf(int (*func)(void *, char), void *farg, const char *fmt, ...);
__format_printf(2, 3) int sprintf(char *__restrict buf, const char *__restrict fmt, ...);
__format_printf(3, 4) int snprintf(
    char *__restrict buf, size_t size, const char *__restrict fmt, ...
);
__format_printf(1, 2) int printf(const char *fmt, ...);
__format_printf(2, 3) int fprintf(FILE *__restrict stream, const char *__restrict fmt, ...);
int vcprintf(int (*func)(void *, char), void *farg, const char *fmt, va_list args);
int vsprintf(char *__restrict buf, const char *__restrict fmt, va_list args);
int vsnprintf(char *__restrict buf, size_t size, const char *__restrict fmt, va_list args);
int vprintf(const char *fmt, va_list args);
int vfprintf(FILE *__restrict fp, const char *__restrict fmt, va_list args);
int scanf(const char *fmt, ...);
int sscanf(const char *__restrict str, const char *__restrict fmt, ...);
int putchar(int ch);
int puts(const char *str);
int putc(int ch, FILE *stream);
int fputc(int ch, FILE *stream);
int fputs(const char *__restrict str, FILE *__restrict stream);
int fgetc(FILE *stream);
int ungetc(int ch, FILE *stream);
char *fgets(char *__restrict str, int num, FILE *__restrict stream);
char *gets(char *str);
int fclose(FILE *stream);
__malloc_like(fclose) FILE *fopen(const char *__restrict path, const char *__restrict mode);
__malloc_like(fclose) FILE *fopendevice(const char *device_name);
__malloc_like(fclose) FILE *fopencookie(
    void *__restrict cookie, const char *__restrict mode, cookie_io_functions_t io_funcs
);
FILE *freopen(const char *__restrict path, const char *__restrict mode, FILE *__restrict stream);
int freopendevice(const char *__restrict device_name, FILE *__restrict stream);
int freopencookie(
    void *__restrict cookie,
    const char *__restrict mode,
    cookie_io_functions_t io_funcs,
    FILE *__restrict stream
);
int fflush(FILE *stream);
size_t fread(void *__restrict ptr, size_t size, size_t count, FILE *__restrict stream);
size_t fwrite(const void *__restrict ptr, size_t size, size_t count, FILE *__restrict stream);
int fseek(FILE *stream, long offset, int origin);
long ftell(FILE *stream);
int feof(FILE *stream);
void clearerr(FILE *stream);
int ferror(FILE *stream);
void perror(const char *str);

#endif  // __STDIO_H__
