#ifndef __STRATA_PLAT_MMU_H__
#define __STRATA_PLAT_MMU_H__

#include <strata/status.h>
#include <strata/mm.h>

struct StMmuP_AddressSpace {
    struct StMmuP_AddressSpace *next;

    St_PhysFrame root_table_pfn;
};

StStatus StMmuP_Init(void);
StStatus StMmuP_LateInit(void);

StStatus StMmuP_CreateAddressSpace(struct StMmuP_AddressSpace **asp __out);
StStatus StMmuP_RemoveAddressSpace(struct StMmuP_AddressSpace *asp __in);
StStatus StMmuP_SwitchAddressSpace(struct StMmuP_AddressSpace *asp __in);

StStatus StMmuP_VirtPageToPhysFrame(
    St_VirtPage vpn __in,
    St_PhysFrame *pfn __out_optional
);
StStatus StMmuP_MapMemory(
    St_PhysFrame pfn __in,
    St_VirtPage vpn __in,
    StMm_MapFlags mapflags __in
);
void StMmuP_UnmapMemory(St_VirtPage vpn __in);

#endif // __STRATA_PLAT_MMU_H__
