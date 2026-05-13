#ifndef __INTERNAL_H__
#define __INTERNAL_H__

#include <stdio.h>

struct file_internal {
    int error;
    int type;

    union {
        struct {
            struct filesystem *fs;
            struct fs_file *file;
        } file;

        struct {
            struct device *dev;
            const struct char_interface *charif;
        } dev;

        struct {
            void *cookie;
            cookie_io_functions_t io_funcs;
        } cookie;
    };
};

#endif  // __INTERNAL_H__
