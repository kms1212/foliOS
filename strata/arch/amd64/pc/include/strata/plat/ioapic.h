#ifndef __STRATA_PLAT_IOAPIC_H__
#define __STRATA_PLAT_IOAPIC_H__

#include <stdint.h>

#include <strata/compiler.h>
#include <strata/status.h>

#define RFLAGS_DLV_MODE_MASK  (0x7 << 0)
#define RFLAGS_DLV_MODE_SHIFT 0

#define DLV_MODE_NORMAL       0
#define DLV_MODE_LOW_PRIORITY 1
#define DLV_MODE_SMI          2
#define DLV_MODE_NMI          4
#define DLV_MODE_INIT         5
#define DLV_MODE_EXTERNAL     7

#define RFLAGS_DEST_LOGICAL    (1 << 3)
#define RFLAGS_APIC_BUSY       (1 << 4)
#define RFLAGS_ACTIVE_LOW      (1 << 5)
#define RFLAGS_LAPIC_RECEIVED  (1 << 6)
#define RFLAGS_LEVEL_TRIGGERED (1 << 7)
#define RFLAGS_INT_MASK        (1 << 8)

StStatus StIoapicP_Add(uint8_t id __in, uint32_t gsi_base __in, uintptr_t mmio_base_phys __in);

StStatus StIoapicP_Read(uint8_t ioapic_idx __in, uint8_t reg __in, uint32_t *value __out);
StStatus StIoapicP_Write(uint8_t ioapic_idx __in, uint8_t reg __in, uint32_t value __in);

StStatus StIoapicP_GetIndexFromGsi(uint32_t gsi __in, uint8_t *ioapic_idx __out);
StStatus StIoapicP_GetIndexAndPinFromGsi(
    uint32_t gsi __in, uint8_t *ioapic_idx __out, uint8_t *pin __out
);
StStatus StIoapicP_RouteGsiToVector(
    uint32_t gsi __in, uint8_t vector __in, uint8_t dest __in, uint16_t flags __in
);

void StIoapicP_Mask(int num __in);
void StIoapicP_Unmask(int num __in);
void StIoapicP_SendEoi(int num __in);

#endif  // __STRATA_PLAT_IOAPIC_H__
