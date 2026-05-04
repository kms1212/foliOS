#ifndef __STRATA_PLAT_PIC_H__
#define __STRATA_PLAT_PIC_H__

#include <stdint.h>

#include <strata/arch/io.h>

#include <strata/compiler.h>

void StPicP_Remap(uint8_t master, uint8_t slave);
void StPicP_Mask(int num);
void StPicP_Unmask(int num);
void StPicP_SendEoi(int num);
void StPicP_Disable(void);

#endif  // __STRATA_PLAT_PIC_H__
