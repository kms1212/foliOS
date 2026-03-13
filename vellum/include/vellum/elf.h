#ifndef __VELLUM_ELF_H__
#define __VELLUM_ELF_H__

#include <stdint.h>
#include <stdio.h>

#include <vellum/arch/elf.h>

#include <vellum/compiler.h>
#include <vellum/status.h>

typedef uint32_t elf32_addr_t;
typedef uint16_t elf32_half_t;
typedef uint32_t elf32_off_t;
typedef int32_t elf32_sword_t;
typedef uint32_t elf32_word_t;
typedef uint16_t elf32_versym_t;

typedef uint64_t elf64_addr_t;
typedef uint16_t elf64_half_t;
typedef int16_t elf64_shalf_t;
typedef uint64_t elf64_off_t;
typedef int32_t elf64_sword_t;
typedef uint32_t elf64_word_t;
typedef uint64_t elf64_xword_t;
typedef uint64_t elf64_sxword_t;
typedef uint16_t elf64_versym_t;

#define ELFMAG "\177ELF"

/* These constants define the different elf file types */
#define ET_NONE   0
#define ET_REL    1
#define ET_EXEC   2
#define ET_DYN    3
#define ET_CORE   4
#define ET_LOPROC 0xff00
#define ET_HIPROC 0xffff

#define ELFCLASSNONE 0
#define ELFCLASS32   1
#define ELFCLASS64   2
#define ELFCLASSNUM  3

#define ELFDATANONE 0
#define ELFDATA2LSB 1
#define ELFDATA2MSB 2

#define EV_NONE    0
#define EV_CURRENT 1
#define EV_NUM     2

#define ELFOSABI_NONE   0x00
#define ELFOSABI_VELLUM 0x13
#define ELFOSABI_EMOS   0x14

struct elf_ident {
    char magic[4];
    uint8_t class;
    uint8_t endianness;
    uint8_t header_version;
    uint8_t osabi;
    uint8_t abi_version;
    uint8_t pad[7];
} __packed;

#define EM_NONE          0x00
#define EM_M32           0x01
#define EM_SPARC         0x02
#define EM_68K           0x04
#define EM_88K           0x05
#define EM_IAMCU         0x06
#define EM_860           0x07
#define EM_MIPS          0x08
#define EM_S370          0x09
#define EM_MIPS_RS3_LE   0x0A
#define EM_PARISC        0x0F
#define EM_VPP500        0x11
#define EM_SPARC32PLUS   0x12
#define EM_960           0x13
#define EM_PPC           0x14
#define EM_PPC64         0x15
#define EM_S390          0x16
#define EM_SPU           0x17
#define EM_V800          0x24
#define EM_FR20          0x25
#define EM_RH32          0x26
#define EM_RCE           0x27
#define EM_ARM           0x28
#define EM_FAKE_ALPHA    0x29
#define EM_SH            0x2A
#define EM_SPARCV9       0x2B
#define EM_TRICORE       0x2C
#define EM_ARC           0x2D
#define EM_H8_300        0x2E
#define EM_H8_300H       0x2F
#define EM_H8S           0x30
#define EM_H8_500        0x31
#define EM_IA_64         0x32
#define EM_MIPS_X        0x33
#define EM_COLDFIRE      0x34
#define EM_M68HC12       0x35
#define EM_MMA           0x36
#define EM_PCP           0x37
#define EM_NCPU          0x38
#define EM_NDR1          0x39
#define EM_STARCORE      0x3A
#define EM_ME16          0x3B
#define EM_ST100         0x3C
#define EM_TINYJ         0x3D
#define EM_X86_64        0x3E
#define EM_PDSP          0x3F
#define EM_PDP10         0x40
#define EM_PDP11         0x41
#define EM_FX66          0x42
#define EM_ST9PLUS       0x43
#define EM_ST7           0x44
#define EM_MC68HC16      0x45
#define EM_MC68HC11      0x46
#define EM_MC68HC08      0x47
#define EM_MC68HC05      0x48
#define EM_SVX           0x49
#define EM_ST19          0x4A
#define EM_VAX           0x4B
#define EM_CRIS          0x4C
#define EM_JAVELIN       0x4D
#define EM_FIREPATH      0x4E
#define EM_ZSP           0x4F
#define EM_MMIX          0x50
#define EM_HUANY         0x51
#define EM_PRISM         0x52
#define EM_AVR           0x53
#define EM_FR30          0x54
#define EM_D10V          0x55
#define EM_D30V          0x56
#define EM_V850          0x57
#define EM_M32R          0x58
#define EM_MN10300       0x59
#define EM_MN10200       0x5A
#define EM_PJ            0x5B
#define EM_OPENRISC      0x5C
#define EM_ARC_COMPACT   0x5D
#define EM_XTENSA        0x5E
#define EM_VIDEOCORE     0x5F
#define EM_TMM_GPP       0x60
#define EM_NS32K         0x61
#define EM_TPC           0x62
#define EM_SNP1K         0x63
#define EM_ST200         0x64
#define EM_IP2K          0x65
#define EM_MAX           0x66
#define EM_CR            0x67
#define EM_F2MC16        0x68
#define EM_MSP430        0x69
#define EM_BLACKFIN      0x6A
#define EM_SE_C33        0x6B
#define EM_SEP           0x6C
#define EM_ARCA          0x6D
#define EM_UNICORE       0x6E
#define EM_EXCESS        0x6F
#define EM_DXP           0x70
#define EM_ALTERA_NIOS2  0x71
#define EM_CRX           0x72
#define EM_XGATE         0x73
#define EM_C166          0x74
#define EM_M16C          0x75
#define EM_DSPIC30F      0x76
#define EM_CE            0x77
#define EM_M32C          0x78
#define EM_M32C          0x78
#define EM_TSK3000       0x83
#define EM_RS08          0x84
#define EM_SHARC         0x85
#define EM_ECOG2         0x86
#define EM_SCORE7        0x87
#define EM_DSP24         0x88
#define EM_VIDEOCORE3    0x89
#define EM_LATTICEMICO32 0x8A
#define EM_SE_C17        0x8B
#define EM_TI_C6000      0x8C
#define EM_TI_C2000      0x8D
#define EM_TI_C5500      0x8E
#define EM_TI_ARP32      0x8F
#define EM_TI_PRU        0x90
#define EM_MMDSP_PLUS    0xA0
#define EM_CYPRESS_M8C   0xA1
#define EM_R32C          0xA2
#define EM_TRIMEDIA      0xA3
#define EM_QDSP6         0xA4
#define EM_8051          0xA5
#define EM_STXP7X        0xA6
#define EM_NDS32         0xA7
#define EM_ECOG1X        0xA8
#define EM_MAXQ30        0xA9
#define EM_XIMO16        0xAA
#define EM_MANIK         0xAB
#define EM_CRAYNV2       0xAC
#define EM_RX            0xAD
#define EM_METAG         0xAE
#define EM_MCST_ELBRUS   0xAF
#define EM_ECOG16        0xB0
#define EM_CR16          0xB1
#define EM_ETPU          0xB2
#define EM_SLE9X         0xB3
#define EM_L10M          0xB4
#define EM_K10M          0xB5
#define EM_AARCH64       0xB7
#define EM_AVR32         0xB9
#define EM_STM8          0xBA
#define EM_TILE64        0xBB
#define EM_TILEPRO       0xBC
#define EM_MICROBLAZE    0xBD
#define EM_CUDA          0xBE
#define EM_TILEGX        0xBF
#define EM_CLOUDSHIELD   0xC0
#define EM_COREA_1ST     0xC1
#define EM_COREA_2ND     0xC2
#define EM_ARCV2         0xC3
#define EM_OPEN8         0xC4
#define EM_RL78          0xC5
#define EM_VIDEOCORE5    0xC6
#define EM_78KOR         0xC7
#define EM_56800EX       0xC8
#define EM_BA1           0xC9
#define EM_BA2           0xCA
#define EM_XCORE         0xCB
#define EM_MCHP_PIC      0xCC
#define EM_INTELGT       0xCD
#define EM_KM32          0xD2
#define EM_KMX32         0xD3
#define EM_EMX16         0xD4
#define EM_EMX8          0xD5
#define EM_KVARC         0xD6
#define EM_CDP           0xD7
#define EM_CDGE          0xD8
#define EM_COOL          0xD9
#define EM_NORC          0xDA
#define EM_CSR_KALIMBA   0xDB
#define EM_Z80           0xDC
#define EM_VISIUM        0xDD
#define EM_FT32          0xDE
#define EM_MOXIE         0xDF
#define EM_AMDGPU        0xE0
#define EM_RISCV         0xF3
#define EM_BPF           0xF7
#define EM_CSKY          0xFC
#define EM_65C816        0x101
#define EM_LOONGARCH     0x102
#define EM_ALPHA         0x9026

struct elf32_ehdr {
    struct elf_ident ident;

    elf32_half_t type;
    elf32_half_t machine;
    elf32_word_t version;
    elf32_addr_t entry;
    elf32_off_t phoff;
    elf32_off_t shoff;
    elf32_word_t flags;
    elf32_half_t ehsize;
    elf32_half_t phentsize;
    elf32_half_t phnum;
    elf32_half_t shentsize;
    elf32_half_t shnum;
    elf32_half_t shstrndx;
} __packed;

struct elf64_ehdr {
    struct elf_ident ident;

    elf64_half_t type;
    elf64_half_t machine;
    elf64_word_t version;
    elf64_addr_t entry;
    elf64_off_t phoff;
    elf64_off_t shoff;
    elf64_word_t flags;
    elf64_half_t ehsize;
    elf64_half_t phentsize;
    elf64_half_t phnum;
    elf64_half_t shentsize;
    elf64_half_t shnum;
    elf64_half_t shstrndx;
} __packed;

#define PT_NULL         0          /* Program header table entry unused */
#define PT_LOAD         1          /* Loadable program segment */
#define PT_DYNAMIC      2          /* Dynamic linking information */
#define PT_INTERP       3          /* Program interpreter */
#define PT_NOTE         4          /* Auxiliary information */
#define PT_SHLIB        5          /* Reserved */
#define PT_PHDR         6          /* Entry for header table itself */
#define PT_TLS          7          /* Thread-local storage segment */
#define PT_NUM          8          /* Number of defined types */
#define PT_LOOS         0x60000000 /* Start of OS-specific */
#define PT_GNU_EH_FRAME 0x6474e550 /* GCC .eh_frame_hdr segment */
#define PT_GNU_STACK    0x6474e551 /* Indicates stack executability */
#define PT_GNU_RELRO    0x6474e552 /* Read-only after relocation */
#define PT_LOSUNW       0x6ffffffa
#define PT_SUNWBSS      0x6ffffffa /* Sun Specific segment */
#define PT_SUNWSTACK    0x6ffffffb /* Stack segment */
#define PT_HISUNW       0x6fffffff
#define PT_HIOS         0x6fffffff /* End of OS-specific */
#define PT_LOPROC       0x70000000 /* Start of processor-specific */
#define PT_HIPROC       0x7fffffff /* End of processor-specific */

/* These constants define the permissions on sections in the program
   header, p_flags. */
#define PF_X        (1 << 0)   /* Segment is executable */
#define PF_W        (1 << 1)   /* Segment is writable */
#define PF_R        (1 << 2)   /* Segment is readable */
#define PF_MASKOS   0x0ff00000 /* OS-specific */
#define PF_MASKPROC 0xf0000000 /* Processor-specific */

struct elf32_phdr {
    elf32_word_t type;
    elf32_off_t offset;
    elf32_addr_t vaddr;
    elf32_addr_t paddr;
    elf32_word_t filesz;
    elf32_word_t memsz;
    elf32_word_t flags;
    elf32_word_t addralign;
} __packed;
;

struct elf64_phdr {
    elf32_word_t type;
    elf64_word_t flags;
    elf64_off_t offset;
    elf64_addr_t vaddr;
    elf64_addr_t paddr;
    elf64_xword_t filesz;
    elf64_xword_t memsz;
    elf64_xword_t addralign;
} __packed;

/* sh_type */
#define SHT_NULL          0
#define SHT_PROGBITS      1
#define SHT_SYMTAB        2
#define SHT_STRTAB        3
#define SHT_RELA          4
#define SHT_HASH          5
#define SHT_DYNAMIC       6
#define SHT_NOTE          7
#define SHT_NOBITS        8
#define SHT_REL           9
#define SHT_SHLIB         10
#define SHT_DYNSYM        11
#define SHT_NUM           12
#define SHT_INIT_ARRAY    14
#define SHT_FINI_ARRAY    15
#define SHT_PREINIT_ARRAY 16
#define SHT_GROUP         17
#define SHT_SYMTAB_SHNDX  18
#define SHT_LOPROC        0x70000000
#define SHT_HIPROC        0x7fffffff
#define SHT_LOUSER        0x80000000
#define SHT_HIUSER        0xffffffff

/* sh_flags */
#define SHF_WRITE            0x1
#define SHF_ALLOC            0x2
#define SHF_EXECINSTR        0x4
#define SHF_MERGE            0x10
#define SHF_STRINGS          0x20
#define SHF_INFO_LINK        0x40
#define SHF_LINK_ORDER       0x80
#define SHF_OS_NONCONFORMING 0x100
#define SHF_GROUP            0x200
#define SHF_TLS              0x400
#define SHF_RELA_LIVEPATCH   0x00100000
#define SHF_RO_AFTER_INIT    0x00200000
#define SHF_ORDERED          0x04000000
#define SHF_EXCLUDE          0x08000000
#define SHF_MASKOS           0x0ff00000
#define SHF_MASKPROC         0xf0000000

/* special section indexes */
#define SHN_UNDEF     0
#define SHN_LORESERVE 0xff00
#define SHN_LOPROC    0xff00
#define SHN_HIPROC    0xff1f
#define SHN_LIVEPATCH 0xff20
#define SHN_ABS       0xfff1
#define SHN_COMMON    0xfff2
#define SHN_HIRESERVE 0xffff

struct elf32_shdr {
    elf32_word_t name;
    elf32_word_t type;
    elf32_word_t flags;
    elf32_addr_t address;
    elf32_off_t offset;
    elf32_word_t size;
    elf32_word_t link;
    elf32_word_t info;
    elf32_word_t addralign;
    elf32_word_t entry_size;
} __packed;

struct elf64_shdr {
    elf64_word_t name;
    elf64_word_t type;
    elf64_xword_t flags;
    elf64_addr_t address;
    elf64_off_t offset;
    elf64_xword_t size;
    elf64_word_t link;
    elf64_word_t info;
    elf64_xword_t addralign;
    elf64_xword_t entry_size;
} __packed;

#define ELF32_ST_BIND(info)       ((info) >> 4)
#define ELF32_ST_TYPE(info)       ((info) & 0xf)
#define ELF32_ST_INFO(bind, type) (((bind) << 4) + ((type) & 0xf))

#define ELF64_ST_BIND(info)       ((info) >> 4)
#define ELF64_ST_TYPE(info)       ((info) & 0xf)
#define ELF64_ST_INFO(bind, type) (((bind) << 4) + ((type) & 0xf))

#define STB_LOCAL  0
#define STB_GLOBAL 1
#define STB_WEAK   2
#define STB_LOOS   10
#define STB_HIOS   12
#define STB_LOPROC 13
#define STB_HIPROC 15

#define STT_NOTYPE  0
#define STT_OBJECT  1
#define STT_FUNC    2
#define STT_SECTION 3
#define STT_FILE    4
#define STT_COMMON  5
#define STT_LOOS    10
#define STT_HIOS    12
#define STT_LOPROC  13
#define STT_HIPROC  15

struct elf32_sym {
    elf32_word_t name;
    elf32_addr_t value;
    elf32_word_t size;
    uint8_t info;
    uint8_t other;
    elf32_half_t shndx;
} __packed;

struct elf64_sym {
    elf64_word_t name;
    uint8_t info;
    uint8_t other;
    elf64_half_t shndx;
    elf64_addr_t value;
    elf64_xword_t size;
} __packed;

/* The following are used with relocations */
#define ELF32_R_SYM(x)  ((x) >> 8)
#define ELF32_R_TYPE(x) ((x) & 0xff)

#define ELF64_R_SYM(i)  ((i) >> 32)
#define ELF64_R_TYPE(i) ((i) & 0xffffffff)

struct elf32_rel {
    elf32_addr_t offset;
    elf32_word_t info;
} __packed;

struct elf64_rel {
    elf64_addr_t offset;
    elf64_xword_t info;
} __packed;

struct elf32_rela {
    elf32_addr_t offset;
    elf32_word_t info;
    elf32_sword_t addend;
} __packed;

struct elf64_rela {
    elf64_addr_t offset;
    elf64_xword_t info;
    elf64_sxword_t addend;
} __packed;

struct elf32_note {
    elf32_word_t namesz;
    elf32_word_t descsz;
    elf32_word_t type;
} __packed;

struct elf64_note {
    elf64_word_t namesz;
    elf64_word_t descsz;
    elf64_word_t type;
} __packed;

struct elf_file {
    FILE *fp;
    union {
        struct elf_ident ident;
        struct elf32_ehdr ehdr32;
        struct elf64_ehdr ehdr64;
    };
    char *shstrtab;
    char *strtab;
    union {
        struct elf32_sym *symtab32;
        struct elf64_sym *symtab64;
    };
    size_t symtab_size;
};

status_t VlElf_Open(const char *path, struct elf_file **elf);
void VlElf_Close(struct elf_file *elf);

status_t VlElf_GetHeader(struct elf_file *elf, void *buf, size_t len);

status_t VlElf_GetProgramHeader(struct elf_file *elf, unsigned int index, void *buf, size_t lne);
status_t VlElf_LoadProgram(struct elf_file *elf, unsigned int index, void *paddr);

status_t VlElf_GetSectionHeader(struct elf_file *elf, unsigned int index, void *buf, size_t len);
status_t VlElf_GetSectionName(struct elf_file *elf, unsigned int index, const char **name);
status_t VlElf_FindSection(struct elf_file *elf, const char *name, unsigned int *idx);
status_t VlElf_LoadSection(struct elf_file *elf, unsigned int index, void *buf, size_t len);

status_t VlElf_FindSymbol(struct elf_file *elf, const char *name, unsigned int *index);
status_t VlElf_GetSymbol(struct elf_file *elf, unsigned int index, void *buf, size_t len);
status_t VlElf_GetSymbolCount(struct elf_file *elf, unsigned int *count);

#endif  // __VELLUM_ELF_H__
