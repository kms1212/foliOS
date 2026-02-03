#ifndef __STRATA_ELF_H__
#define __STRATA_ELF_H__

#include <stddef.h>
#include <stdint.h>

#include <strata/arch/elf.h>

#include <strata/compiler.h>
#include <strata/status.h>

typedef uint32_t StElf32_Addr;
typedef uint16_t StElf32_Half;
typedef uint32_t StElf32_Off;
typedef int32_t StElf32_SWord;
typedef uint32_t StElf32_Word;
typedef uint16_t StElf32_VerSym;

typedef uint64_t StElf64_Addr;
typedef uint16_t StElf64_Half;
typedef int16_t StElf64_SHalf;
typedef uint64_t StElf64_Off;
typedef int32_t StElf64_SWord;
typedef uint32_t StElf64_Word;
typedef uint64_t StElf64_XWord;
typedef uint64_t StElf64_SXWord;
typedef uint16_t StElf64_VerSym;

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
#define ELFOSABI_STRATA 0x14

struct StElf_Ident {
    char magic[4];
    uint8_t class;
    uint8_t endianness;
    uint8_t header_version;
    uint8_t osabi;
    uint8_t abi_version;
    uint8_t pad[7];
} __packed;

#define EM_NONE 0x00

struct StElf32_Ehdr {
    struct StElf_Ident ident;

    StElf32_Half type;
    StElf32_Half machine;
    StElf32_Word version;
    StElf32_Addr entry;
    StElf32_Off phoff;
    StElf32_Off shoff;
    StElf32_Word flags;
    StElf32_Half ehsize;
    StElf32_Half phentsize;
    StElf32_Half phnum;
    StElf32_Half shentsize;
    StElf32_Half shnum;
    StElf32_Half shstrndx;
} __packed;

struct StElf64_Ehdr {
    struct StElf_Ident ident;

    StElf64_Half type;
    StElf64_Half machine;
    StElf64_Word version;
    StElf64_Addr entry;
    StElf64_Off phoff;
    StElf64_Off shoff;
    StElf64_Word flags;
    StElf64_Half ehsize;
    StElf64_Half phentsize;
    StElf64_Half phnum;
    StElf64_Half shentsize;
    StElf64_Half shnum;
    StElf64_Half shstrndx;
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

struct StElf32_Phdr {
    StElf32_Word type;
    StElf32_Off offset;
    StElf32_Addr vaddr;
    StElf32_Addr paddr;
    StElf32_Word filesz;
    StElf32_Word memsz;
    StElf32_Word flags;
    StElf32_Word addralign;
} __packed;

struct StElf64_Phdr {
    StElf32_Word type;
    StElf64_Word flags;
    StElf64_Off offset;
    StElf64_Addr vaddr;
    StElf64_Addr paddr;
    StElf64_XWord filesz;
    StElf64_XWord memsz;
    StElf64_XWord addralign;
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

struct StElf32_Shdr {
    StElf32_Word name;
    StElf32_Word type;
    StElf32_Word flags;
    StElf32_Addr address;
    StElf32_Off offset;
    StElf32_Word size;
    StElf32_Word link;
    StElf32_Word info;
    StElf32_Word addralign;
    StElf32_Word entry_size;
} __packed;

struct StElf64_Shdr {
    StElf64_Word name;
    StElf64_Word type;
    StElf64_XWord flags;
    StElf64_Addr address;
    StElf64_Off offset;
    StElf64_XWord size;
    StElf64_Word link;
    StElf64_Word info;
    StElf64_XWord addralign;
    StElf64_XWord entry_size;
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

struct StElf32_Sym {
    StElf32_Word name;
    StElf32_Addr value;
    StElf32_Word size;
    uint8_t info;
    uint8_t other;
    StElf32_Half shndx;
} __packed;

struct StElf64_Sym {
    StElf64_Word name;
    uint8_t info;
    uint8_t other;
    StElf64_Half shndx;
    StElf64_Addr value;
    StElf64_XWord size;
} __packed;

/* The following are used with relocations */
#define ELF32_R_SYM(x)  ((x) >> 8)
#define ELF32_R_TYPE(x) ((x) & 0xff)

#define ELF64_R_SYM(i)  ((i) >> 32)
#define ELF64_R_TYPE(i) ((i) & 0xffffffff)

struct StElf32_Rel {
    StElf32_Addr offset;
    StElf32_Word info;
} __packed;

struct StElf64_Rel {
    StElf64_Addr offset;
    StElf64_XWord info;
} __packed;

struct StElf32_Rela {
    StElf32_Addr offset;
    StElf32_Word info;
    StElf32_SWord addend;
} __packed;

struct StElf64_Rela {
    StElf64_Addr offset;
    StElf64_XWord info;
    StElf64_SXWord addend;
} __packed;

struct StElf32_Note {
    StElf32_Word namesz;
    StElf32_Word descsz;
    StElf32_Word type;
} __packed;

struct StElf64_Note {
    StElf64_Word namesz;
    StElf64_Word descsz;
    StElf64_Word type;
} __packed;

#define AT_NULL       0
#define AT_IGNORE     1
#define AT_EXECFD     2
#define AT_PHDR       3
#define AT_PHENT      4
#define AT_PHNUM      5
#define AT_PAGESZ     6
#define AT_BASE       7
#define AT_ENTRY      9
#define AT_NOTELF     10
#define AT_UID        11
#define AT_EUID       12
#define AT_GID        13
#define AT_EGID       14
#define AT_PLATFORM   15
#define AT_HWCAP      16
#define AT_CLKTCK     17
#define AT_RANDOM     25
#define AT_STRATA_KRT 0x10000

struct StElf32_Auxv {
    StElf32_Word type;
    StElf32_Word val;
} __packed;

struct StElf64_Auxv {
    StElf64_XWord type;
    StElf64_XWord val;
} __packed;

struct StElf_Object {
    const void *img_base;
    size_t img_size;
    union {
        struct StElf_Ident ident;
        struct StElf32_Ehdr ehdr32;
        struct StElf64_Ehdr ehdr64;
    };
    size_t symtab_size;
};

StStatus StElf_Open(
    const void *img_base __in, size_t img_size __in, struct StElf_Object **elf __out
);
// StStatus StElf_Open(const char *path __in, struct StElf_Object **elf __out);
void StElf_Close(struct StElf_Object *elf __in);

StStatus StElf_GetHeader(struct StElf_Object *elf __in, void *buf __buf, size_t len __in);
StStatus StElf_GetEntryPoint(struct StElf_Object *elf __in, uintptr_t *entry_point __out);

StStatus StElf_GetProgramHeaderCount(struct StElf_Object *elf __in, unsigned int *count __out);
StStatus StElf_GetProgramHeader(
    struct StElf_Object *elf __in, unsigned int index __in, void *buf __buf, size_t len __in
);
StStatus StElf_LoadProgram(struct StElf_Object *elf __in, unsigned int index __in);

#endif  // __STRATA_ELF_H__
