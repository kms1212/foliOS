#ifndef __LOADST_RAMDISK_H__
#define __LOADST_RAMDISK_H__

#include <stdint.h>

#include <loadst/compiler.h>

struct ramdisk_header {
    uint32_t rootdir_offset;
};

#define RDET_END       0
#define RDET_DIRECTORY 1
#define RDET_FILE      2

struct ramdisk_direntry {
    uint8_t type;
    uint8_t name_len;
    uint16_t entry_size;

    union {
        struct {
            uint32_t file_size;
            uint32_t file_crc32;
            uint32_t file_offset;
        } __packed file;

        struct {
            uint32_t unused1;
            uint32_t unused2;
            uint32_t file_offset;
        } __packed directory;
    } __packed;

    char name[];
} __packed;

#endif  // __LOADST_RAMDISK_H__
