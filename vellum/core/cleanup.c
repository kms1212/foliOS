#include <stddef.h>

#include <vellum/device.h>

extern void (*__fini_array_start)(void);
extern void (*__fini_array_end)(void);

extern void _pc_cleanup(void);

void Vl_Cleanup(void)
{
    struct device *dev = VlDev_GetFirst();
    struct device *last_root_dev = NULL;

    while (dev) {
        if (!dev->parent) {
            if (last_root_dev) {
                last_root_dev->driver->remove(last_root_dev);
            }

            last_root_dev = dev;
        }

        dev = dev->next;
    }

    if (last_root_dev) {
        last_root_dev->driver->remove(last_root_dev);
    }

    for (int i = 0; &(&__fini_array_start)[i] != &__fini_array_end; i++) {
        (&__fini_array_start)[i]();
    }

    _pc_cleanup();
}
