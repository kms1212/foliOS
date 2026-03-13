#include <vellum/device.h>

#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vellum/status.h>

static struct device *device_list_head = NULL;

struct device *VlDev_GetFirst(void)
{
    return device_list_head;
}

status_t VlDev_Create(struct device **devout, struct device_driver *drv, struct device *parent)
{
    struct device *dev;

    dev = calloc(1, sizeof(*dev));
    if (!dev) return STATUS_UNKNOWN_ERROR;

    dev->parent = parent;
    dev->driver = drv;

    if (!device_list_head) {
        device_list_head = dev;
    } else {
        struct device *current = device_list_head;
        for (; current->next; current = current->next) {
        }
        current->next = dev;
    }

    if (dev->parent) {
        if (!dev->parent->first_child) {
            dev->parent->first_child = dev;
        } else {
            struct device *current = dev->parent->first_child;
            for (; current->sibling; current = current->sibling) {
            }
            current->sibling = dev;
        }
    }

    if (devout) *devout = dev;

    return STATUS_SUCCESS;
}

void VlDev_Remove(struct device *dev)
{
    if (!device_list_head) return;

    struct device *prev_device = NULL, *prev_sibling = NULL;
    for (struct device *current = device_list_head; current->next; current = current->next) {
        if (current->next == dev) {
            prev_device = current;
        }

        if (current->sibling == dev) {
            prev_sibling = current;
        }

        if ((!dev->next || prev_device) && (!dev->sibling || prev_sibling)) {
            break;
        }
    }

    if (device_list_head == dev) {
        device_list_head = dev->next;
    }

    if (!prev_device) return;

    prev_device->next = dev->next;

    if (prev_sibling) {
        prev_sibling->sibling = dev->sibling;
    }

    free(dev);
}

status_t VlDev_Find(const char *id, struct device **dev)
{
    for (struct device *current = device_list_head; current; current = current->next) {
        if (strncmp(current->name, id, sizeof(current->name)) == 0) {
            if (dev) *dev = current;
            return STATUS_SUCCESS;
        }
    }

    return STATUS_ENTRY_NOT_FOUND;
}

status_t VlDev_GenerateName(const char *basename, char *buf, size_t len)
{
    int id_max = -1;
    size_t basename_len = strlen(basename);
    char *name_end;
    int id;

    for (struct device *current = device_list_head; current; current = current->next) {
        if (strncmp(basename, current->name, basename_len) != 0) {
            continue;
        }

        id = strtol(current->name + basename_len, &name_end, 10);
        if (*name_end) {
            continue;
        }

        if (id_max < id) {
            id_max = id;
        }
    }

    snprintf(buf, len, "%s%d", basename, id_max + 1);

    return STATUS_SUCCESS;
}

static struct device_driver *driver_list_head = NULL;

status_t VlDev_CreateDriver(struct device_driver **drvout)
{
    struct device_driver *drv;

    drv = calloc(1, sizeof(*drv));
    if (!drv) return STATUS_UNKNOWN_ERROR;

    if (!driver_list_head) {
        driver_list_head = drv;
    } else {
        struct device_driver *current = driver_list_head;
        for (; current->next; current = current->next) {
        }

        current->next = drv;
    }

    if (drvout) *drvout = drv;

    return STATUS_SUCCESS;
}

status_t VlDev_FindDriver(const char *name, struct device_driver **drv)
{
    for (struct device_driver *current = driver_list_head; current; current = current->next) {
        if (strcmp(name, current->name) == 0) {
            if (drv) *drv = current;
            return STATUS_SUCCESS;
        }
    }

    return STATUS_ENTRY_NOT_FOUND;
}
