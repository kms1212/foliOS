#include <strata/mm/utils.h>

#include <stddef.h>
#include <stdint.h>

#include <strata/plat/mm.h>

#include <strata/compiler.h>
#include <strata/status.h>

StStatus StMm_ReadLocal(
    StMm_AddressSpace_StrongRef asp __in, uintptr_t addr __in, void *buf __buf, size_t len __in
)
{
    return StMmP_ReadLocal(asp, addr, buf, len);
}

StStatus StMm_WriteLocal(
    StMm_AddressSpace_StrongRef asp __in, uintptr_t addr __in, const void *buf __in, size_t len __in
)
{
    return StMmP_WriteLocal(asp, addr, buf, len);
}

StStatus StMm_SetLocal(
    StMm_AddressSpace_StrongRef asp __in, uintptr_t addr __in, int value, size_t len __in
)
{
    return StMmP_SetLocal(asp, addr, value, len);
}

StStatus StMm_CopyLocal(
    StMm_AddressSpace_StrongRef dest_asp __in,
    uintptr_t dest __in,
    StMm_AddressSpace_StrongRef src_asp __in,
    uintptr_t src __in,
    size_t len __in
)
{
    return StMmP_CopyLocal(dest_asp, dest, src_asp, src, len);
}
