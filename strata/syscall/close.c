#include <strata/syscall.h>

#include <stdint.h>

#include <strata/compiler.h>
#include <strata/handle.h>
#include <strata/status.h>

StStatus StSyscall_Close(uint32_t handle __in)
{
    return StHandle_Close((StHandle)handle);
}
