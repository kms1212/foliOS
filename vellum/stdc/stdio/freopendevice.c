#include <stdio.h>

#include <vellum/device.h>

int freopendevice(const char *__restrict device_name, FILE *__restrict stream)
{
    status_t status;
    struct device *dev;

    status = VlDev_Find(device_name, &dev);
    if (!CHECK_SUCCESS(status)) return 1;

    stream->type = 2;
    stream->dev.dev = dev;
    status = dev->driver->get_interface(dev, "char", (const void **)&stream->dev.charif);
    if (!CHECK_SUCCESS(status)) return 1;

    return 0;
}
