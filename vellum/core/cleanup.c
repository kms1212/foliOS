#include <stddef.h>

#include <vellum/device.h>

extern void (*_fini_array_start_)(void);
extern void (*_fini_array_end_)(void);

extern void _pc_cleanup(void);

void cleanup(void)
{
    struct device *dev = device_get_first_dev();
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

    for (int i = 0; &(&_fini_array_start_)[i] != &_fini_array_end_; i++) {
        (&_fini_array_start_)[i]();
    }

    _pc_cleanup();
}
