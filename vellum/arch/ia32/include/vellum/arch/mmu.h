#ifndef __VELLUM_ASM_MMU_H__
#define __VELLUM_ASM_MMU_H__

#include <stdint.h>

#include <vellum/compiler.h>

#ifndef PAGE_SIZE
#    define PAGE_SIZE 4096

#endif

union VlA_PageDirEntry {
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
    } dir __packed;

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
    } pse __packed;
} __packed;

union VlA_PageTableEntry {
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

struct page_dir_recursive {
    union VlA_PageDirEntry pde[1023];
    union VlA_PageTableEntry pte;
} __packed;

#endif  // __VELLUM_ASM_MMU_H__
