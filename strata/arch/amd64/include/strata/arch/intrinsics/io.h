#ifndef __STRATA_ARCH_INTRINSICS_IO_H__
#define __STRATA_ARCH_INTRINSICS_IO_H__

#include <assert.h>
#include <stdint.h>

#include <strata/compiler.h>

__always_inline void StIoA_Out8(uint16_t port __in, uint8_t value __in)
{
    __asm__ volatile("outb %b0, %w1" : : "a"(value), "Nd"(port));
}

__always_inline void StIoA_Out16(uint16_t port __in, uint16_t value __in)
{
    __asm__ volatile("outw %w0, %w1" : : "a"(value), "Nd"(port));
}

__always_inline void StIoA_Out32(uint16_t port __in, uint32_t value __in)
{
    __asm__ volatile("outl %0, %w1" : : "a"(value), "Nd"(port));
}

__always_inline void StIoA_Outs8(uint16_t port __in, const uint8_t *data __in, unsigned long count __in)
{
    __asm__ volatile("rep outsb" : "+S"(data), "+c"(count) : "d"(port) : "memory");
}

__always_inline void StIoA_Outs16(uint16_t port __in, const uint16_t *data __in, unsigned long count __in)
{
    __asm__ volatile("rep outsw" : "+S"(data), "+c"(count) : "d"(port) : "memory");
}

__always_inline void StIoA_Outs32(uint16_t port __in, const uint32_t *data __in, unsigned long count __in)
{
    __asm__ volatile("rep outsl" : "+S"(data), "+c"(count) : "d"(port) : "memory");
}

__always_inline uint8_t StIoA_In8(uint16_t port __in)
{
    uint8_t value;
    __asm__ volatile("inb %w1, %b0" : "=a"(value) : "Nd"(port));
    return value;
}

__always_inline uint16_t StIoA_In16(uint16_t port __in)
{
    uint16_t value;
    __asm__ volatile("inw %w1, %w0" : "=a"(value) : "Nd"(port));
    return value;
}

__always_inline uint32_t StIoA_In32(uint16_t port __in)
{
    uint32_t value;
    __asm__ volatile("inl %w1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

__always_inline void StIoA_Ins8(uint16_t port __in, uint8_t *data __out, unsigned long count __in)
{
    assert(data);

    __asm__ volatile("rep insb" : "+D"(data), "+c"(count) : "d"(port) : "memory");
}

__always_inline void StIoA_Ins16(uint16_t port __in, uint16_t *data __out, unsigned long count __in)
{
    assert(data);

    __asm__ volatile("rep insw" : "+D"(data), "+c"(count) : "d"(port) : "memory");
}

__always_inline void StIoA_Ins32(uint16_t port __in, uint32_t *data __out, unsigned long count __in)
{
    assert(data);

    __asm__ volatile("rep insl" : "+D"(data), "+c"(count) : "d"(port) : "memory");
}

#endif  // __STRATA_ARCH_INTRINSICS_IO_H__
