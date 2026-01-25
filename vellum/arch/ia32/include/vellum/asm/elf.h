#ifndef __VELLUM_ASM_ELF_H__
#define __VELLUM_ASM_ELF_H__

#define EM_386 0x03

#define R_386_NONE     0
#define R_386_32       1
#define R_386_PC32     2
#define R_386_GOT32    3
#define R_386_PLT32    4
#define R_386_COPY     5
#define R_386_GLOB_DAT 6
#define R_386_JMP_SLOT 7
#define R_386_RELATIVE 8
#define R_386_GOTOFF   9
#define R_386_GOTPC    10
#define R_386_16       20
#define R_386_PC16     21
#define R_386_8        22
#define R_386_PC8      23
#define R_386_SIZE32   38
#define R_386_GOT32X   43

#endif  // __VELLUM_ASM_ELF_H__
