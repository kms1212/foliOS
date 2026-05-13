#include <vellum/plat/panic.h>
#include <vellum/status.h>

void abort(void)
{
    VlP_Panic(STATUS_UNKNOWN_ERROR, "abort");
}
