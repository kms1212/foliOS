#ifndef __VELLUM_ASM_PIC_H__
#define __VELLUM_ASM_PIC_H__

#include <stdint.h>

void VlPicP_RemapInterrupt(uint8_t master, uint8_t slave);

void VlPicP_Mask(int num);
void VlPicP_Unmask(int num);

#endif  // __VELLUM_ASM_PIC_H__
