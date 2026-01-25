#ifndef __VELLUM_ASM_PAGE_H__
#define __VELLUM_ASM_PAGE_H__

#include <stdint.h>

#include <vellum/compiler.h>

#define PAGE_SIZE 4096

union page_dir_entry {
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

union page_table_entry {
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
    union page_dir_entry pde[1023];
    union page_table_entry pte;
} __packed;

#endif  // __VELLUM_ASM_PAGE_H__
