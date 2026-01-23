#include <stdint.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include <vellum/asm/io.h>
#include <vellum/asm/isr.h>
#include <vellum/asm/instruction.h>
#include <vellum/asm/intrinsics/rdtsc.h>

#include <vellum/device.h>
#include <vellum/global_configs.h>
#include <vellum/interface/rtc.h>
#include <vellum/interface/nvram.h>

#define RTC_NMI_DISABLE         0x80

#define RTC_REG_SECONDS         0x00
#define RTC_REG_SECONDS_ALARM   0x01
#define RTC_REG_MINUTES         0x02
#define RTC_REG_MINUTES_ALARM   0x03
#define RTC_REG_HOURS           0x04
#define RTC_REG_HOURS_ALARM     0x05
#define RTC_REG_WEEKDAY         0x06
#define RTC_REG_MONTHDAY        0x07
#define RTC_REG_MONTH           0x08
#define RTC_REG_YEAR            0x09
#define RTC_REG_STATUS_A        0x0a
#define RTC_REG_STATUS_B        0x0b
#define RTC_REG_STATUS_C        0x0c
#define RTC_REG_STATUS_D        0x0d
#define RTC_REG_RESET_CODE      0x0f

#define RTC_A_UIP 0x80

#define RTC_B_SET  0x80
#define RTC_B_PIE  0x40
#define RTC_B_AIE  0x20
#define RTC_B_UIE  0x10
#define RTC_B_BIN  0x04
#define RTC_B_24HR 0x02
#define RTC_B_DSE  0x01

struct rtc_data {
    int io_index, io_data;
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
        io_out8(data->io_index, RTC_NMI_DISABLE | RTC_REG_STATUS_A);
    } while (io_in8(data->io_data) & RTC_A_UIP);

retry:
    io_out8(data->io_index, RTC_NMI_DISABLE | RTC_REG_SECONDS);
    tm->second = bcd2int(io_in8(data->io_data));

    io_out8(data->io_index, RTC_NMI_DISABLE | RTC_REG_MINUTES);
    tm->minute = bcd2int(io_in8(data->io_data));

    io_out8(data->io_index, RTC_NMI_DISABLE | RTC_REG_HOURS);
    tm->hour = bcd2int(io_in8(data->io_data));

    io_out8(data->io_index, RTC_NMI_DISABLE | RTC_REG_MONTHDAY);
    tm->mday = bcd2int(io_in8(data->io_data));

    io_out8(data->io_index, RTC_NMI_DISABLE | RTC_REG_MONTH);
    tm->month = bcd2int(io_in8(data->io_data));

    if (config_rtc_century_offset) {
        io_out8(data->io_index, RTC_NMI_DISABLE | config_rtc_century_offset);
        century = bcd2int(io_in8(data->io_data));
    }

    io_out8(data->io_index, RTC_NMI_DISABLE | RTC_REG_YEAR);
    tm->year = century * 100 + bcd2int(io_in8(data->io_data));

    io_out8(data->io_index, RTC_NMI_DISABLE | RTC_REG_SECONDS);
    if (tm->second != bcd2int(io_in8(data->io_data))) goto retry;

    if (!_pc_rdtsc_undefined && data->tsc_diff_per_second) {
        tm->millisecond = (uint32_t)((_ia32_rdtsc() - data->prev_tsc_value) >> 16) * 1000 / data->tsc_diff_per_second;
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
    return STATUS_UNSUPPORTED;
}

status_t get_alarm(struct device *dev, struct rtc_time *tm)
{
    return STATUS_UNSUPPORTED;
}

status_t set_alarm(struct device *dev, const struct rtc_time *tm)
{
    return STATUS_UNSUPPORTED;
}

static const struct rtc_interface rtcif = {
    .get_time = get_time,
    .set_time = set_time,
    .get_alarm = get_alarm,
    .set_alarm = set_alarm,
};

status_t read_nvram(struct device *dev, int offset, uint8_t *val)
{
    return STATUS_UNSUPPORTED;
}

status_t write_nvram(struct device *dev, int offset, uint8_t val)
{
    return STATUS_UNSUPPORTED;
}

static const struct nvram_interface nvrif = {
    .read_nvram = read_nvram,
    .write_nvram = write_nvram,
};

static void rtc_isr(void *_dev, struct interrupt_frame *frame, struct trap_regs *regs, int num)
{
    struct device *dev = (struct device *)_dev;
    struct rtc_data *data = dev->data;
    uint8_t regc;

    io_out8(data->io_index, RTC_NMI_DISABLE | RTC_REG_STATUS_C);
    regc = io_in8(data->io_data);

    if (!_pc_rdtsc_undefined && (regc & 0x10)) {
        if (data->prev_tsc_value) {
            data->tsc_diff_per_second = (_ia32_rdtsc() - data->prev_tsc_value) >> 16;
        }

        data->prev_tsc_value = _ia32_rdtsc();
    }
}

static status_t probe(struct device **devout, struct device_driver *drv, struct device *parent, struct resource *rsrc, int rsrc_cnt);
static status_t remove(struct device *dev);
static status_t get_interface(struct device *dev, const char *name, const void **result);
    
static void rtc_isa_init(void)
{
    status_t status;
    struct device_driver *drv;

    status = device_driver_create(&drv);
    if (!CHECK_SUCCESS(status)) {
        panic(status, "cannot register device driver \"rtc_isa\"");
    }

    drv->name = "rtc_isa";
    drv->probe = probe;
    drv->remove = remove;
    drv->get_interface = get_interface;
}

static status_t probe(struct device **devout, struct device_driver *drv, struct device *parent, struct resource *rsrc, int rsrc_cnt)
{
    status_t status;
    struct device *dev = NULL;
    struct rtc_data *data = NULL;

    if (!rsrc || rsrc_cnt != 2 ||
        rsrc[0].type != RT_IOPORT || rsrc[0].limit - rsrc[0].base != 1 ||
        rsrc[1].type != RT_IRQ || rsrc[1].base != rsrc[1].limit) {
        status = STATUS_INVALID_RESOURCE;
        goto has_error;
    }

    status = device_create(&dev, drv, parent);
    if (!CHECK_SUCCESS(status)) goto has_error;

    status = device_generate_name("rtc", dev->name, sizeof(dev->name));
    if (!CHECK_SUCCESS(status)) goto has_error;

    data = malloc(sizeof(*data));
    if (!data) {
        status = STATUS_UNKNOWN_ERROR;
        goto has_error;
    }

    data->io_index = rsrc[0].base;
    data->io_data = rsrc[0].limit;
    data->irq_num = rsrc[1].base;
    data->prev_tsc_value = 0;
    data->tsc_diff_per_second = 0;
    dev->data = data;

    status = _pc_isr_mask_interrupt(rsrc[1].base);
    if (!CHECK_SUCCESS(status)) goto has_error;

    status = _pc_isr_add_interrupt_handler(data->irq_num, dev, rtc_isr, &data->isr);
    if (!CHECK_SUCCESS(status)) goto has_error;

    // enable update interrupt
    io_out8(data->io_index, RTC_NMI_DISABLE | RTC_REG_STATUS_B);
    uint8_t temp = io_in8(data->io_data);
    io_out8(data->io_index, RTC_NMI_DISABLE | RTC_REG_STATUS_B);
    io_out8(data->io_data, temp | RTC_B_UIE | RTC_B_24HR);

    status = _pc_isr_unmask_interrupt(rsrc[1].base);
    if (!CHECK_SUCCESS(status)) goto has_error;
    
    if (devout) *devout = dev;

    return STATUS_SUCCESS;

has_error:
    _pc_isr_unmask_interrupt(rsrc[1].base);

    if (data && data->isr) {
        _pc_isr_remove_handler(data->isr);
    }

    if (data) {
        free(data);
    }

    if (dev) {
        device_remove(dev);
    }

    return status;
}

static status_t remove(struct device *dev)
{
    struct rtc_data *data = (struct rtc_data *)dev->data;

    _pc_isr_remove_handler(data->isr);

    free(data);

    device_remove(dev);

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

