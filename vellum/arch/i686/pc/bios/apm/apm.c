#include <vellum/asm/apm.h>

#include <vellum/asm/bios/apm.h>
#include <vellum/asm/bios/bioscall.h>
#include <vellum/asm/farptr.h>
#include <vellum/asm/pc_gdt.h>

status_t _pc_apm_init(void)
{
    return STATUS_SUCCESS;
}
