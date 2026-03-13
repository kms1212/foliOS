#ifndef __VELLUM_ARCH_INTRINSICS_IO_H__
#define __VELLUM_ARCH_INTRINSICS_IO_H__

#include <stdint.h>

#include <vellum/compiler.h>

__always_inline void VlA_Out8(uint16_t port, uint8_t value)
{
    __asm__ volatile("outb %b0, %w1" : : "a"(value), "Nd"(port));
}

__always_inline void VlA_Out16(uint16_t port, uint16_t value)
{
    __asm__ volatile("outw %w0, %w1" : : "a"(value), "Nd"(port));
}

__always_inline void VlA_Out32(uint16_t port, uint32_t value)
{
    __asm__ volatile("outl %0, %w1" : : "a"(value), "Nd"(port));
}

__always_inline void VlA_Outs8(uint16_t port, const uint8_t *data, unsigned long count)
{
    __asm__ volatile("rep outsb" : "+S"(data), "+c"(count) : "d"(port) : "memory");
}

__always_inline void VlA_Outs16(uint16_t port, const uint16_t *data, unsigned long count)
{
    __asm__ volatile("rep outsw" : "+S"(data), "+c"(count) : "d"(port) : "memory");
}

__always_inline void VlA_Outs32(uint16_t port, const uint32_t *data, unsigned long count)
{
    __asm__ volatile("rep outsl" : "+S"(data), "+c"(count) : "d"(port) : "memory");
}

__always_inline uint8_t VlA_In8(uint16_t port)
{
    uint8_t value;
    __asm__ volatile("inb %w1, %b0" : "=a"(value) : "Nd"(port));
    return value;
}

__always_inline uint16_t VlA_In16(uint16_t port)
{
    uint16_t value;
    __asm__ volatile("inw %w1, %w0" : "=a"(value) : "Nd"(port));
    return value;
}

__always_inline uint32_t VlA_In32(uint16_t port)
{
    uint32_t value;
    __asm__ volatile("inl %w1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

__always_inline void VlA_Ins8(uint16_t port, uint8_t *data, unsigned long count)
{
    __asm__ volatile("rep insb" : "+D"(data), "+c"(count) : "d"(port) : "memory");
}

__always_inline void VlA_Ins16(uint16_t port, uint16_t *data, unsigned long count)
{
    __asm__ volatile("rep insw" : "+D"(data), "+c"(count) : "d"(port) : "memory");
}

__always_inline void VlA_Ins32(uint16_t port, uint32_t *data, unsigned long count)
{
    __asm__ volatile("rep insl" : "+D"(data), "+c"(count) : "d"(port) : "memory");
}

#endif  // __VELLUM_ARCH_INTRINSICS_IO_H__
