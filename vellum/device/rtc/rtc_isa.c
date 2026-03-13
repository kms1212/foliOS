#include "vellum/arch/cpufeatures.h"
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vellum/arch/intrinsics/rdtsc.h>
#include <vellum/arch/io.h>

#include <vellum/plat/instruction.h>
#include <vellum/plat/isr.h>

#include <vellum/device.h>
#include <vellum/global_configs.h>
#include <vellum/interface/nvram.h>
#include <vellum/interface/rtc.h>

#define RTC_NMI_DISABLE 0x80

#define RTC_REG_SECONDS       0x00
#define RTC_REG_SECONDS_ALARM 0x01
#define RTC_REG_MINUTES       0x02
#define RTC_REG_MINUTES_ALARM 0x03
#define RTC_REG_HOURS         0x04
#define RTC_REG_HOURS_ALARM   0x05
#define RTC_REG_WEEKDAY       0x06
#define RTC_REG_MONTHDAY      0x07
#define RTC_REG_MONTH         0x08
#define RTC_REG_YEAR          0x09
#define RTC_REG_STATUS_A      0x0a
#define RTC_REG_STATUS_B      0x0b
#define RTC_REG_STATUS_C      0x0c
#define RTC_REG_STATUS_D      0x0d
#define RTC_REG_RESET_CODE    0x0f

#define RTC_A_UIP 0x80

#define RTC_B_SET  0x80
#define RTC_B_PIE  0x40
#define RTC_B_AIE  0x20
#define RTC_B_UIE  0x10
#define RTC_B_BIN  0x04
#define RTC_B_24HR 0x02
#define RTC_B_DSE  0x01

struct rtc_data {
    int VlA_Index, io_data;
    int irq_num;
    uint32_t tsc_diff_per_second;
    uint64_t prev_tsc_value;
    struct isr_handler *isr;
};

static uint8_t bcd2int(uint8_t bcd)
{
    return ((bcd >> 4) & 0xF) * 10 + (bcd & 0xF);
}

status_t get_time(struct device *dev, struct rtc_time *tm)
{
    struct rtc_data *data = (struct rtc_data *)dev->data;
    int century = 20;

    do {
        VlA_Out8(data->VlA_Index, RTC_NMI_DISABLE | RTC_REG_STATUS_A);
    } while (VlA_In8(data->io_data) & RTC_A_UIP);

retry:
    VlA_Out8(data->VlA_Index, RTC_NMI_DISABLE | RTC_REG_SECONDS);
    tm->second = bcd2int(VlA_In8(data->io_data));

    VlA_Out8(data->VlA_Index, RTC_NMI_DISABLE | RTC_REG_MINUTES);
    tm->minute = bcd2int(VlA_In8(data->io_data));

    VlA_Out8(data->VlA_Index, RTC_NMI_DISABLE | RTC_REG_HOURS);
    tm->hour = bcd2int(VlA_In8(data->io_data));

    VlA_Out8(data->VlA_Index, RTC_NMI_DISABLE | RTC_REG_MONTHDAY);
    tm->mday = bcd2int(VlA_In8(data->io_data));

    VlA_Out8(data->VlA_Index, RTC_NMI_DISABLE | RTC_REG_MONTH);
    tm->month = bcd2int(VlA_In8(data->io_data));

    if (config_rtc_century_offset) {
        VlA_Out8(data->VlA_Index, RTC_NMI_DISABLE | config_rtc_century_offset);
        century = bcd2int(VlA_In8(data->io_data));
    }

    VlA_Out8(data->VlA_Index, RTC_NMI_DISABLE | RTC_REG_YEAR);
    tm->year = century * 100 + bcd2int(VlA_In8(data->io_data));

    VlA_Out8(data->VlA_Index, RTC_NMI_DISABLE | RTC_REG_SECONDS);
    if (tm->second != bcd2int(VlA_In8(data->io_data))) goto retry;

    if (!g_p_cpu_features->has_tsc && data->tsc_diff_per_second) {
        tm->millisecond =
            (int)(((VlA_Rdtsc() - data->prev_tsc_value) >> 16) * 1000 / data->tsc_diff_per_second);
        if (tm->millisecond >= 1000) {
            tm->millisecond = 999;
        }
    } else {
        tm->millisecond = 0;
    }

    return STATUS_SUCCESS;
}

status_t set_time(struct device *dev, const struct rtc_time *tm)
{
    return STATUS_NOT_SUPPORTED;
}

status_t get_alarm(struct device *dev, struct rtc_time *tm)
{
    return STATUS_NOT_SUPPORTED;
}

status_t set_alarm(struct device *dev, const struct rtc_time *tm)
{
    return STATUS_NOT_SUPPORTED;
}

static const struct rtc_interface rtcif = {
    .get_time = get_time,
    .set_time = set_time,
    .get_alarm = get_alarm,
    .set_alarm = set_alarm,
};

status_t read_nvram(struct device *dev, int offset, uint8_t *val)
{
    return STATUS_NOT_SUPPORTED;
}

status_t write_nvram(struct device *dev, int offset, uint8_t val)
{
    return STATUS_NOT_SUPPORTED;
}

static const struct nvram_interface nvrif = {
    .read_nvram = read_nvram,
    .write_nvram = write_nvram,
};

static void rtc_isr(void *_dev, struct VlA_InterruptFrame *frame, struct trap_regs *regs, int num)
{
    struct device *dev = (struct device *)_dev;
    struct rtc_data *data = dev->data;
    uint8_t regc;

    VlA_Out8(data->VlA_Index, RTC_NMI_DISABLE | RTC_REG_STATUS_C);
    regc = VlA_In8(data->io_data);

    if (!g_p_cpu_features->has_tsc && (regc & 0x10)) {
        if (data->prev_tsc_value) {
            data->tsc_diff_per_second = (VlA_Rdtsc() - data->prev_tsc_value) >> 16;
        }

        data->prev_tsc_value = VlA_Rdtsc();
    }
}

static status_t probe(
    struct device **devout,
    struct device_driver *drv,
    struct device *parent,
    struct resource *rsrc,
    int rsrc_cnt
);
static status_t remove(struct device *dev);
static status_t get_interface(struct device *dev, const char *name, const void **result);

static void rtc_isa_init(void)
{
    status_t status;
    struct device_driver *drv;

    status = VlDev_CreateDriver(&drv);
    if (!CHECK_SUCCESS(status)) {
        VlP_Panic(status, "cannot register device driver \"rtc_isa\"");
    }

    drv->name = "rtc_isa";
    drv->probe = probe;
    drv->remove = remove;
    drv->get_interface = get_interface;
}

static status_t probe(
    struct device **devout,
    struct device_driver *drv,
    struct device *parent,
    struct resource *rsrc,
    int rsrc_cnt
)
{
    status_t status;
    struct device *dev = NULL;
    struct rtc_data *data = NULL;

    if (!rsrc || rsrc_cnt != 2 || rsrc[0].type != RT_IOPORT || rsrc[0].limit - rsrc[0].base != 1 ||
        rsrc[1].type != RT_IRQ || rsrc[1].base != rsrc[1].limit) {
        return STATUS_INVALID_RESOURCE;
    }

    status = VlDev_Create(&dev, drv, parent);
    if (!CHECK_SUCCESS(status)) goto has_error;

    status = VlDev_GenerateName("rtc", dev->name, sizeof(dev->name));
    if (!CHECK_SUCCESS(status)) goto has_error;

    data = calloc(1, sizeof(*data));
    if (!data) {
        status = STATUS_UNKNOWN_ERROR;
        goto has_error;
    }

    data->VlA_Index = (int)rsrc[0].base;
    data->io_data = (int)rsrc[0].limit;
    data->irq_num = (int)rsrc[1].base;
    data->prev_tsc_value = 0;
    data->tsc_diff_per_second = 0;
    dev->data = data;

    status = VlIntP_Mask(data->irq_num);
    if (!CHECK_SUCCESS(status)) goto has_error;

    status = VlIntP_AddInterruptHandler(data->irq_num, dev, rtc_isr, &data->isr);
    if (!CHECK_SUCCESS(status)) goto has_error;

    // enable update interrupt
    VlA_Out8(data->VlA_Index, RTC_NMI_DISABLE | RTC_REG_STATUS_B);
    uint8_t temp = VlA_In8(data->io_data);
    VlA_Out8(data->VlA_Index, RTC_NMI_DISABLE | RTC_REG_STATUS_B);
    VlA_Out8(data->io_data, temp | RTC_B_UIE | RTC_B_24HR);

    status = VlIntP_Unmask(data->irq_num);
    if (!CHECK_SUCCESS(status)) goto has_error;

    if (devout) *devout = dev;

    return STATUS_SUCCESS;

has_error:
    VlIntP_Unmask((int)rsrc[1].base);

    if (data && data->isr) {
        VlIntP_RemoveHandler(data->isr);
    }

    if (data) {
        free(data);
    }

    if (dev) {
        VlDev_Remove(dev);
    }

    return status;
}

static status_t remove(struct device *dev)
{
    struct rtc_data *data = (struct rtc_data *)dev->data;

    VlIntP_RemoveHandler(data->isr);

    free(data);

    VlDev_Remove(dev);

    return STATUS_SUCCESS;
}

static status_t get_interface(struct device *dev, const char *name, const void **result)
{
    if (strcmp(name, "rtc") == 0) {
        if (result) *result = &rtcif;
        return STATUS_SUCCESS;
    } else if (strcmp(name, "nvram") == 0) {
        if (result) *result = &nvrif;
        return STATUS_SUCCESS;
    }

    return STATUS_ENTRY_NOT_FOUND;
}

REGISTER_DEVICE_DRIVER(rtc_isa, rtc_isa_init)
