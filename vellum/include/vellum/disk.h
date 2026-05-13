#ifndef __VELLUM_DISK_H__
#define __VELLUM_DISK_H__

#include <stdint.h>

#include <vellum/compiler.h>

struct chs {
    int cylinder, head, sector;
};

typedef int64_t lba_t;

__always_inline lba_t VlDisk_ChsToLba(struct chs chs, struct chs geom)
{
    return ((((lba_t)chs.cylinder * geom.head) + chs.head) * geom.sector) + chs.sector - 1;
}

__always_inline struct chs VlDisk_LbaToChs(lba_t lba, struct chs geom)
{
    struct chs chs;
    chs.sector = (int)((lba % geom.sector) + 1);
    chs.head = (int)((lba / geom.sector) % geom.head);
    chs.cylinder = (int)(lba / geom.sector / geom.head);

    return chs;
}

#endif  // __VELLUM_DISK_H__
