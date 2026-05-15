#include <vellum/filesystem.h>

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <vellum/device.h>
#include <vellum/status.h>

static struct filesystem *fs_list_head = NULL;

struct filesystem *VlFs_GetFirst(void)
{
    return fs_list_head;
}

VlStatus VlFs_Create(
    struct filesystem **fsout, struct fs_driver *drv, struct device *dev, const char *name
)
{
    VlStatus status;
    struct filesystem *fs;
    struct filesystem *conflicting_fs;

    status = VlFs_Find(name, &conflicting_fs);
    if (CHECK_SUCCESS(status)) return STATUS_DUPLICATE_ENTRY;

    fs = calloc(1, sizeof(*fs));
    if (!fs) return STATUS_UNKNOWN_ERROR;

    fs->driver = drv;
    fs->dev = dev;

    if (!fs_list_head) {
        fs_list_head = fs;
    } else {
        struct filesystem *current = fs_list_head;
        for (; current->next; current = current->next) {
        }

        current->next = fs;
    }

    fs->next = NULL;
    strncpy(fs->name, name, sizeof(fs->name) - 1);

    if (fsout) *fsout = fs;

    return STATUS_SUCCESS;
}

void VlFs_Remove(struct filesystem *fs)
{
    struct filesystem *prev_fs = NULL;

    if (!fs_list_head) return;

    for (struct filesystem *current = fs_list_head; current->next; current = current->next) {
        if (current->next == fs) {
            prev_fs = current;
        }
    }
    if (!prev_fs) return;

    prev_fs->next = fs->next;

    free(fs);
}

VlStatus VlFs_Find(const char *name, struct filesystem **fsout)
{
    for (struct filesystem *current = fs_list_head; current; current = current->next) {
        if (strncmp(current->name, name, sizeof(current->name)) == 0) {
            if (fsout) *fsout = current;
            return STATUS_SUCCESS;
        }
    }

    return STATUS_ENTRY_NOT_FOUND;
}

static struct fs_driver *driver_list_head = NULL;

VlStatus VlFs_CreateDriver(struct fs_driver **drvout)
{
    struct fs_driver *drv;

    drv = calloc(1, sizeof(*drv));
    if (!drv) return STATUS_UNKNOWN_ERROR;

    if (!driver_list_head) {
        driver_list_head = drv;
    } else {
        struct fs_driver *current = driver_list_head;
        for (; current->next; current = current->next) {
        }

        current->next = drv;
    }

    if (drvout) *drvout = drv;

    return STATUS_SUCCESS;
}

VlStatus VlFs_FindDriver(const char *name, struct fs_driver **drv)
{
    for (struct fs_driver *current = driver_list_head; current; current = current->next) {
        if (strcmp(name, current->name) == 0) {
            if (drv) *drv = current;
            return STATUS_SUCCESS;
        }
    }

    return STATUS_ENTRY_NOT_FOUND;
}

VlStatus VlFs_MountAuto(struct device *__restrict dev, const char *__restrict name)
{
    VlStatus status;

    for (struct fs_driver *current = driver_list_head; current; current = current->next) {
        status = current->probe(dev, current);
        if (!CHECK_SUCCESS(status)) continue;

        status = current->mount(NULL, current, dev, name);
        if (CHECK_SUCCESS(status)) return status;
    }

    return STATUS_NOT_SUPPORTED;
}
