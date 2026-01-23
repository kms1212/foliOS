#ifndef __STRATA_ARCH_PAGE_H__
#define __STRATA_ARCH_PAGE_H__

#include <stdint.h>

#include <strata/compiler.h>

#define PAGE_SIZE 4096

union StA_PageTableEntry {
    uint32_t raw;

    struct {
        uint32_t p : 1;
        uint32_t r_w : 1;
        uint32_t u_s : 1;
        uint32_t pwt : 1;
        uint32_t pcd : 1;
        uint32_t a : 1;
        uint32_t d : 1;
        uint32_t pat : 1;
        uint32_t g : 1;
        uint32_t avl : 3;
        uint32_t base : 20;
    } __packed;
} __packed;

union StA_PageDirectoryEntry {
    uint32_t raw;
    
    struct {
        uint32_t p : 1;
        uint32_t r_w : 1;
        uint32_t u_s : 1;
        uint32_t pwt : 1;
        uint32_t pcd : 1;
        uint32_t a : 1;
        uint32_t avl2 : 1;
        uint32_t ps : 1;
        uint32_t avl1 : 4;
        uint32_t base : 20;
    } __packed;

    struct {
        uint32_t p : 1;
        uint32_t r_w : 1;
        uint32_t u_s : 1;
        uint32_t pwt : 1;
        uint32_t pcd : 1;
        uint32_t a : 1;
        uint32_t d : 1;
        uint32_t ps : 1;
        uint32_t g : 1;
        uint32_t avl : 3;
        uint32_t pat : 1;
        uint32_t base_high : 8;
        uint32_t : 1;
        uint32_t base_low : 10;
    } __packed huge;

    union StA_PageTableEntry recursive;
} __packed;

#endif // __STRATA_ARCH_PAGE_H__
