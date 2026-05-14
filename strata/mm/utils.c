#include <strata/mm/utils.h>

#include <stddef.h>
#include <stdint.h>

#include <strata/plat/mm.h>

#include <strata/compiler.h>
#include <strata/mm/address_space_refs.h>
#include <strata/status.h>

StStatus StMm_ReadLocal(
    StAddressSpace_StrongRef asp __in, uintptr_t addr __in, void *buf __buf, size_t len __in
)
{
    return StMmP_ReadLocal(asp, addr, buf, len);
}

StStatus StMm_WriteLocal(
    StAddressSpace_StrongRef asp __in, uintptr_t addr __in, const void *buf __in, size_t len __in
)
{
    return StMmP_WriteLocal(asp, addr, buf, len);
}

StStatus StMm_SetLocal(
    StAddressSpace_StrongRef asp __in, uintptr_t addr __in, int value, size_t len __in
)
{
    return StMmP_SetLocal(asp, addr, value, len);
}

StStatus StMm_CopyLocal(
    StAddressSpace_StrongRef dest_asp __in,
    uintptr_t dest __in,
    StAddressSpace_StrongRef src_asp __in,
    uintptr_t src __in,
    size_t len __in
)
{
    return StMmP_CopyLocal(dest_asp, dest, src_asp, src, len);
}
