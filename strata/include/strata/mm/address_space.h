#ifndef __STRATA_MM_ADDRESS_SPACE_H__
#define __STRATA_MM_ADDRESS_SPACE_H__

#include <strata/plat/mm.h>

#include <strata/compiler.h>
#include <strata/mm/address_space_refs.h>
#include <strata/process_refs.h>
#include <strata/ref_control.h>
#include <strata/status.h>

/**
 * Ref-counted local user virtual address space.
 *
 * Address spaces own VMM local-domain metadata and platform page-table state.
 * Allocation cleanup is charged through the process allocation owner rather
 * than through the address-space object itself.
 */
struct StAddressSpace {
    /** First-field ref control block used by StAddressSpace_Acquire/Release. */
    struct StRefControlBlock ref_control;

    /** Global address-space list link. */
    StAddressSpace_InternalRef next;

    /** Owning process; acquire before using outside the owning teardown path. */
    StProcess_InternalRef process;

    struct StAddressSpaceP_PlatformData platform_data;

    /** VMM reservation list root for the local user domain. */
    void *user_reservation_head;
    /** Inclusive local user-domain bounds and free count. */
    St_VirtPage user_base_vpn, user_limit_vpn;
    St_PageCount user_free_count;
};

/** Initialize the kernel/base address-space object. */
StStatus StAddressSpace_InitBase(void);

/** Create a process-local address space and return its strong reference. */
StStatus StAddressSpace_Create(
    StAddressSpace_StrongRef *asp __out, StProcess_StrongRef process __in
);
/** Remove an address space after its process allocation owner is closing. */
void StAddressSpace_Remove(StAddressSpace_StrongRef asp __in);
/** Acquire another strong address-space reference. */
void StAddressSpace_Acquire(StAddressSpace_StrongRef asp __inout);
/** Release a strong address-space reference. */
void StAddressSpace_Release(StAddressSpace_StrongRef asp __inout);
/** Switch the current CPU to the address-space platform state. */
StStatus StAddressSpace_Switch(StAddressSpace_StrongRef asp __in);

#endif  // __STRATA_MM_ADDRESS_SPACE_H__
