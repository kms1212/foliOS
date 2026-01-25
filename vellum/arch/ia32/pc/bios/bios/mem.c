#include <vellum/asm/bios/mem.h>

#include <vellum/asm/bios/bioscall.h>

status_t _pc_bios_mem_query_map(uint32_t *_cursor, struct smap_entry *buf, long buf_size)
{
    uint32_t cursor = _cursor ? *_cursor : 0;

    struct bioscall_regs regs = {
        .a.l = 0xE820,
        .b.l = cursor,
        .c.l = buf_size,
        .d.l = 0x534D4150,
        .es.w = ((uintptr_t)buf >> 4) & 0xFFFF,
        .di.w = (uintptr_t)buf & 0x000F,
    };

    if (_pc_bios_call(0x15, &regs)) {
        return STATUS_UNKNOWN_ERROR;
    }

    if (regs.a.l != 0x534D4150) {
        return STATUS_UNSUPPORTED;
    }

    if (_cursor) {
        *_cursor = regs.b.l;
    }

    return STATUS_SUCCESS;
}
