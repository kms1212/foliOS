#ifndef __VELLUM_FS_H__
#define __VELLUM_FS_H__

#include <stdint.h>
#include <stdio.h>

#include <vellum/compiler.h>
#include <vellum/device.h>

#define FILESYSTEM_NAME_MAX 64

struct fs_directory;
struct fs_directory_entry;
struct fs_file;

struct fs_driver {
    struct fs_driver *next;

    const char *name;
    status_t (*probe)(struct device *, struct fs_driver *);
    status_t (*mount)(struct filesystem **, struct fs_driver *, struct device *, const char *);
    status_t (*unmount)(struct filesystem *);

    status_t (*open)(struct fs_directory *, const char *, struct fs_file **);
    status_t (*read)(struct fs_file *, void *, size_t, size_t *);
    status_t (*seek)(struct fs_file *, off_t, int);
    status_t (*tell)(struct fs_file *, off_t *);
    void (*close)(struct fs_file *);

    status_t (*open_root_directory)(struct filesystem *, struct fs_directory **);
    status_t (*open_directory)(struct fs_directory *, const char *, struct fs_directory **);
    status_t (*rewind_directory)(struct fs_directory *);
    status_t (*iter_directory)(struct fs_directory *, struct fs_directory_entry *);
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

status_t VlFs_Create(
    struct filesystem **fsout, struct fs_driver *drv, struct device *dev, const char *name
);
void VlFs_Remove(struct filesystem *fs);

struct filesystem *VlFs_GetFirst(void);
status_t VlFs_Find(const char *name, struct filesystem **fs);

status_t VlFs_CreateDriver(struct fs_driver **drv);

status_t VlFs_FindDriver(const char *name, struct fs_driver **drv);

status_t VlFs_MountAuto(struct device *__restrict dev, const char *__restrict name);
status_t VlFs_Mount(
    struct device *__restrict dev, const char *__restrict fsname, const char *__restrict name
);

#define REGISTER_FILESYSTEM_DRIVER(name, init_func)                                                \
    __constructor static void _register_driver_##name(void)                                        \
    {                                                                                              \
        init_func();                                                                               \
    }

#endif  // __VELLUM_FS_H__
