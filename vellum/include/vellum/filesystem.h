#ifndef __VELLUM_FS_H__
#define __VELLUM_FS_H__

#include <limits.h>
#include <stdint.h>
#include <stdio.h>

#include <vellum/compiler.h>
#include <vellum/device.h>

#define FILESYSTEM_NAME_MAX 64

struct fs_directory;
struct fs_directory_entry;
struct fs_file;
struct filesystem;

struct fs_driver {
    struct fs_driver *next;

    const char *name;
    VlStatus (*probe)(struct device *, struct fs_driver *);
    VlStatus (*mount)(struct filesystem **, struct fs_driver *, struct device *, const char *);
    VlStatus (*unmount)(struct filesystem *);

    VlStatus (*open)(struct fs_directory *, const char *, struct fs_file **);
    VlStatus (*read)(struct fs_file *, void *, size_t, size_t *);
    VlStatus (*seek)(struct fs_file *, off_t, int);
    VlStatus (*tell)(struct fs_file *, off_t *);
    void (*close)(struct fs_file *);

    VlStatus (*open_root_directory)(struct filesystem *, struct fs_directory **);
    VlStatus (*open_directory)(struct fs_directory *, const char *, struct fs_directory **);
    VlStatus (*rewind_directory)(struct fs_directory *);
    VlStatus (*iter_directory)(struct fs_directory *, struct fs_directory_entry *);
    void (*close_directory)(struct fs_directory *);
};

struct filesystem {
    struct filesystem *next;

    struct fs_driver *driver;
    struct device *dev;
    char name[FILESYSTEM_NAME_MAX];

    void *data;
};

struct fs_file {
    struct filesystem *fs;

    void *data;
};

struct fs_directory {
    struct filesystem *fs;

    void *data;
};

struct fs_directory_entry {
    char name[FILENAME_MAX];
    uint64_t size;
};

VlStatus VlFs_Create(
    struct filesystem **fsout, struct fs_driver *drv, struct device *dev, const char *name
);
void VlFs_Remove(struct filesystem *fs);

struct filesystem *VlFs_GetFirst(void);
VlStatus VlFs_Find(const char *name, struct filesystem **fs);

VlStatus VlFs_CreateDriver(struct fs_driver **drv);

VlStatus VlFs_FindDriver(const char *name, struct fs_driver **drv);

VlStatus VlFs_MountAuto(struct device *__restrict dev, const char *__restrict name);
VlStatus VlFs_Mount(
    struct device *__restrict dev, const char *__restrict fsname, const char *__restrict name
);

#define REGISTER_FILESYSTEM_DRIVER(name, init_func)                                                \
    __constructor static void _register_driver_##name(void)                                        \
    {                                                                                              \
        init_func();                                                                               \
    }

#endif  // __VELLUM_FS_H__
