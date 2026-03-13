#include <stdlib.h>

#include <vellum/status.h>

#include <vellum/panic.h>

void abort(void)
{
    VlP_Panic(STATUS_UNKNOWN_ERROR, "abort");
}
