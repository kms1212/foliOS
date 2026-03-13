#ifndef __SETJMP_H__
#define __SETJMP_H__

#include <vellum/plat/setjmp.h>

#include <vellum/compiler.h>

__noreturn void longjmp(jmp_buf jmpbuf, int ret);

int setjmp(jmp_buf jmpbuf);

#endif  // __SETJMP_H__
