#ifndef __STRATA_GNT_H__
#define __STRATA_GNT_H__

#include <stddef.h>

#include <strata/status.h>
#include <strata/utf.h>

struct StGnt_Node {
    void *private_data;

    size_t name_len;
    St_Utf32Char name[];
};

#endif // __STRATA_GNT_H__
