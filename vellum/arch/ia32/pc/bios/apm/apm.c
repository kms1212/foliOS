#include <vellum/plat/apm.h>

#include <vellum/arch/farptr.h>
#include <vellum/arch/gdt.h>

#include <vellum/plat/bios/apm.h>
#include <vellum/plat/bios/bioscall.h>

status_t _pc_apm_init(void)
{
    return STATUS_SUCCESS;
}
