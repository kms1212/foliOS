#ifndef __VELLUM_ASM_IO_H__
#define __VELLUM_ASM_IO_H__

#include <stdint.h>

#include <vellum/asm/intrinsics/io.h>

#include <vellum/compiler.h>

#define io_out8     _ia32_out8
#define io_out16    _ia32_out16
#define io_out32    _ia32_out32

#define io_in8      _ia32_in8
#define io_in16     _ia32_in16
#define io_in32     _ia32_in32

#define io_outs8    _ia32_outs8
#define io_outs16   _ia32_outs16
#define io_outs32   _ia32_outs32

#define io_ins8     _ia32_ins8
#define io_ins16    _ia32_ins16
#define io_ins32    _ia32_ins32

#endif // __VELLUM_ASM_IO_H__
