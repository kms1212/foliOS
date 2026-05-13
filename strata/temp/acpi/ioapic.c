#include <ioapic.h>

#include <assert.h>
#include <stdint.h>

#include <strata/compiler.h>
#include <strata/mm.h>
#include <strata/mm/types.h>
#include <strata/mm/vmm.h>
#include <strata/status.h>

#define IOAPIC_MAX_COUNT 8

#define IOAPIC_MMIO_SEL 0x00
#define IOAPIC_MMIO_WIN 0x10

#define IOAPIC_REG_VER         0x01
#define IOAPIC_REG_REDTBL_BASE 0x10

#define IOAPIC_REG_REDTBL_INT_VECTOR_MASK  (0xFF << 0)
#define IOAPIC_REG_REDTBL_INT_VECTOR_SHIFT 0
#define IOAPIC_REG_REDTBL_DLV_MODE_MASK    (0x7 << 8)
#define IOAPIC_REG_REDTBL_DLV_MODE_SHIFT   0
#define IOAPIC_REG_REDTBL_DEST_LOGICAL     (1 << 11)
#define IOAPIC_REG_REDTBL_APIC_BUSY        (1 << 12)
#define IOAPIC_REG_REDTBL_ACTIVE_LOW       (1 << 13)
#define IOAPIC_REG_REDTBL_LAPIC_RECEIVED   (1 << 14)
#define IOAPIC_REG_REDTBL_LEVEL_TRIGGERED  (1 << 15)
#define IOAPIC_REG_REDTBL_INT_MASK         (1 << 16)
#define IOAPIC_REG_REDTBL_DEST_APIC_MASK   (0xFF << 24)
#define IOAPIC_REG_REDTBL_DEST_APIC_SHIFT  24

struct ioapic_info {
    uint32_t gsi_base;
    uint32_t max_intr;
    uintptr_t mmio_base_phys;
    volatile void *mmio_base;
    uint8_t id;
};

static struct ioapic_info ioapic_arr[IOAPIC_MAX_COUNT];
static uint32_t ioapic_count = 0;

struct vector_mapping_info {
    uint8_t mapped;
    uint8_t ioapic_idx;
    uint8_t pin;
};

static struct vector_mapping_info vector_mapping_info_table[256];

StStatus StIoapicP_Add(uint8_t id __in, uint32_t gsi_base __in, uintptr_t mmio_base_phys __in)
{
    StStatus status;
    St_VirtPage mmio_vpn;
    volatile void *mmio_base;
    uint32_t ver;

    if (ioapic_count >= IOAPIC_MAX_COUNT) return STATUS_NOT_ALLOCATED;

    status = StMm_MapGlobal(
        VMM_DOMAIN_IO,
        &mmio_vpn,
        ADDR_TO_FRAME(mmio_base_phys),
        1,
        NULL,
        (struct StMm_CompoundFlags){AF_DEFAULT, MF_KERNEL_DEFAULT | MF_NO_CACHE}
    );
    if (!CHECK_SUCCESS(status)) return status;

    mmio_base = PAGE_TO_VPTR(mmio_vpn);

    *(volatile uint32_t *)((uintptr_t)mmio_base + IOAPIC_MMIO_SEL) = IOAPIC_REG_VER;
    ver = *(volatile uint32_t *)((uintptr_t)mmio_base + IOAPIC_MMIO_WIN);

    ioapic_arr[ioapic_count].id = id;
    ioapic_arr[ioapic_count].gsi_base = gsi_base;
    ioapic_arr[ioapic_count].mmio_base_phys = mmio_base_phys;
    ioapic_arr[ioapic_count].mmio_base = mmio_base;
    ioapic_arr[ioapic_count].max_intr = (ver >> 16) & 0xFF;
    ioapic_count++;

    return STATUS_SUCCESS;
}

StStatus StIoapicP_Read(uint8_t ioapic_idx __in, uint8_t reg __in, uint32_t *value __out)
{
    assert(value);

    volatile void *mmio_base;

    if (ioapic_idx >= ioapic_count) return STATUS_INVALID_VALUE;

    mmio_base = ioapic_arr[ioapic_idx].mmio_base;

    *(volatile uint32_t *)((uintptr_t)mmio_base + IOAPIC_MMIO_SEL) = reg;
    *value = *(volatile uint32_t *)((uintptr_t)mmio_base + IOAPIC_MMIO_WIN);

    return STATUS_SUCCESS;
}

StStatus StIoapicP_Write(uint8_t ioapic_idx __in, uint8_t reg __in, uint32_t value __in)
{
    volatile void *mmio_base;

    if (ioapic_idx >= ioapic_count) return STATUS_INVALID_VALUE;

    mmio_base = ioapic_arr[ioapic_idx].mmio_base;

    *(volatile uint32_t *)((uintptr_t)mmio_base + IOAPIC_MMIO_SEL) = reg;
    *(volatile uint32_t *)((uintptr_t)mmio_base + IOAPIC_MMIO_WIN) = value;

    return STATUS_SUCCESS;
}

StStatus StIoapicP_RouteGsiToVector(
    uint32_t gsi __in, uint8_t vector __in, uint8_t dest __in, uint16_t flags __in
)
{
    StStatus status;
    uint32_t lo = 0, hi = 0;
    uint8_t ioapic_idx;
    uint8_t pin;

    status = StIoapicP_GetIndexAndPinFromGsi(gsi, &ioapic_idx, &pin);
    if (!CHECK_SUCCESS(status)) return status;

    lo = (uint32_t)vector | ((uint32_t)flags << 8);
    hi = (uint32_t)dest << 24;

    status = StIoapicP_Write(ioapic_idx, IOAPIC_REG_REDTBL_BASE + (pin * 2) + 1, hi);
    if (!CHECK_SUCCESS(status)) return status;

    status = StIoapicP_Write(ioapic_idx, IOAPIC_REG_REDTBL_BASE + (pin * 2), lo);
    if (!CHECK_SUCCESS(status)) return status;

    vector_mapping_info_table[vector].mapped = 1;
    vector_mapping_info_table[vector].ioapic_idx = ioapic_idx;
    vector_mapping_info_table[vector].pin = pin;

    return STATUS_SUCCESS;
}

StStatus StIoapicP_GetIndexFromGsi(uint32_t gsi __in, uint8_t *ioapic_idx __out)
{
    assert(ioapic_idx);

    for (uint32_t i = 0; i < ioapic_count; i++) {
        if (ioapic_arr[i].gsi_base <= gsi &&
            gsi <= ioapic_arr[i].gsi_base + ioapic_arr[i].max_intr) {
            *ioapic_idx = i;

            return STATUS_SUCCESS;
        }
    }
    return STATUS_ENTRY_NOT_FOUND;
}

StStatus StIoapicP_GetIndexAndPinFromGsi(
    uint32_t gsi __in, uint8_t *ioapic_idx __out, uint8_t *pin __out
)
{
    assert(ioapic_idx);
    assert(pin);

    StStatus status;

    status = StIoapicP_GetIndexFromGsi(gsi, ioapic_idx);
    if (!CHECK_SUCCESS(status)) return status;

    *pin = gsi - ioapic_arr[*ioapic_idx].gsi_base;

    return STATUS_SUCCESS;
}

void StIoapicP_Mask(int num __in)
{
    StStatus status;
    uint32_t entry_low;
    uint8_t ioapic_idx;
    uint8_t pin;

    if (!vector_mapping_info_table[num].mapped) return;

    ioapic_idx = vector_mapping_info_table[num].ioapic_idx;
    pin = vector_mapping_info_table[num].pin;

    status = StIoapicP_Read(ioapic_idx, IOAPIC_REG_REDTBL_BASE + (pin * 2), &entry_low);
    if (!CHECK_SUCCESS(status)) return;

    entry_low |= IOAPIC_REG_REDTBL_INT_MASK;

    status = StIoapicP_Write(ioapic_idx, IOAPIC_REG_REDTBL_BASE + (pin * 2), entry_low);
    if (!CHECK_SUCCESS(status)) return;
}

void StIoapicP_Unmask(int num __in)
{
    StStatus status;
    uint32_t entry_low;
    uint8_t ioapic_idx;
    uint8_t pin;

    if (!vector_mapping_info_table[num].mapped) return;

    ioapic_idx = vector_mapping_info_table[num].ioapic_idx;
    pin = vector_mapping_info_table[num].pin;

    status = StIoapicP_Read(ioapic_idx, IOAPIC_REG_REDTBL_BASE + (pin * 2), &entry_low);
    if (!CHECK_SUCCESS(status)) return;

    entry_low &= ~IOAPIC_REG_REDTBL_INT_MASK;

    status = StIoapicP_Write(ioapic_idx, IOAPIC_REG_REDTBL_BASE + (pin * 2), entry_low);
    if (!CHECK_SUCCESS(status)) return;
}
