#include <strata/plat/panic.h>

#include <inttypes.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>

#include <strata/arch/intrinsics/io.h>
#include <strata/arch/intrinsics/misc.h>

#include <strata/plat/cpulocal.h>

#include <strata/compiler.h>
#include <strata/mm/types.h>
#include <strata/status.h>
#include <strata/symbol.h>
#include <strata/thread.h>
#include <strata/thread_refs.h>

#define PANIC_BACKTRACE_MAX_FRAMES 32

static int panic_out(void *data, char ch)
{
    if (!ch) return 1;

    StIoA_Out8(0x00E9, ch);

    return 0;
}

struct panic_stack_frame {
    struct panic_stack_frame *prev;
    uintptr_t return_address;
};

extern char _early_stack_base[];
extern char _early_stack[];

static uintptr_t read_rbp(void)
{
    uintptr_t rbp;
    __asm__ volatile("mov %%rbp, %0" : "=r"(rbp));
    return rbp;
}

static int contains_frame(uintptr_t base, uintptr_t limit, uintptr_t rbp)
{
    if (limit < base || limit - base < sizeof(struct panic_stack_frame)) return 0;
    return rbp >= base && rbp <= limit - sizeof(struct panic_stack_frame);
}

static int get_backtrace_bounds(uintptr_t rbp, uintptr_t *base, uintptr_t *limit)
{
    struct StCpuLocalP_Data *cpu_local = StCpuLocalP_GetData();
    StThread_InternalRef current_thread = NULL;
    uintptr_t stack_base;
    uintptr_t stack_limit;

    if (cpu_local) {
        current_thread = cpu_local->scheduler.current_thread;
    }

    if (current_thread && current_thread->kmode_stack_page_count) {
        stack_base = PAGE_TO_ADDR(current_thread->kmode_stack_base_vpn);
        stack_limit = PAGE_TO_ADDR(
            current_thread->kmode_stack_base_vpn + current_thread->kmode_stack_page_count
        );

        if (contains_frame(stack_base, stack_limit, rbp)) {
            *base = stack_base;
            *limit = stack_limit;
            return 1;
        }
    }

    stack_base = (uintptr_t)_early_stack_base;
    stack_limit = (uintptr_t)_early_stack;
    if (contains_frame(stack_base, stack_limit, rbp)) {
        *base = stack_base;
        *limit = stack_limit;
        return 1;
    }

    return 0;
}

static void print_backtrace(uintptr_t rbp, uintptr_t rip)
{
    uintptr_t base = 0;
    uintptr_t limit = 0;
    size_t frame_index = 0;

    cprintf(panic_out, NULL, "backtrace:\n");

    if (rip) {
        char symbol[128];
        StStatus status = StSymbol_FormatStatic(rip, symbol, sizeof(symbol));
        if (CHECK_SUCCESS(status)) {
            cprintf(panic_out, NULL, "  #%02zu 0x%016" PRIXPTR " <%s>\n", frame_index++, rip, symbol);
        } else {
            cprintf(panic_out, NULL, "  #%02zu 0x%016" PRIXPTR "\n", frame_index++, rip);
        }
    }

    if (!get_backtrace_bounds(rbp, &base, &limit)) {
        cprintf(panic_out, NULL, "  <frame pointer unavailable: 0x%016" PRIXPTR ">\n", rbp);
        return;
    }

    while (frame_index < PANIC_BACKTRACE_MAX_FRAMES && rbp) {
        struct panic_stack_frame *frame = (struct panic_stack_frame *)rbp;
        uintptr_t prev_rbp;

        if (!contains_frame(base, limit, rbp)) {
            cprintf(panic_out, NULL, "  <invalid frame pointer: 0x%016" PRIXPTR ">\n", rbp);
            return;
        }

        {
            char symbol[128];
            uintptr_t call_address = frame->return_address;
            StStatus status;

            if (call_address) call_address--;
            status = StSymbol_FormatStatic(call_address, symbol, sizeof(symbol));
            if (CHECK_SUCCESS(status)) {
                cprintf(
                    panic_out,
                    NULL,
                    "  #%02zu 0x%016" PRIXPTR " <%s>\n",
                    frame_index++,
                    call_address,
                    symbol
                );
            } else {
                cprintf(panic_out, NULL, "  #%02zu 0x%016" PRIXPTR "\n", frame_index++, call_address);
            }
        }

        prev_rbp = (uintptr_t)frame->prev;
        if (!prev_rbp) return;
        if (prev_rbp <= rbp) {
            cprintf(panic_out, NULL, "  <non-monotonic frame pointer: 0x%016" PRIXPTR ">\n", prev_rbp);
            return;
        }

        rbp = prev_rbp;
    }

    if (frame_index == PANIC_BACKTRACE_MAX_FRAMES) {
        cprintf(panic_out, NULL, "  <truncated>\n");
    }
}

static __noreturn void panic_common(StStatus status, uintptr_t rbp, uintptr_t rip, const char *fmt, va_list args)
{
    cprintf(panic_out, NULL, "panic: %" PRIX32 ", ", status);
    vcprintf(panic_out, NULL, fmt, args);
    print_backtrace(rbp, rip);

    StA_Cli();
    for (;;) {
        StA_Hlt();
    }
}

__noreturn void StP_Panic(StStatus status, const char *fmt, ...)
{
    va_list args;
    uintptr_t rbp = read_rbp();

    va_start(args, fmt);
    panic_common(status, rbp, 0, fmt, args);
    va_end(args);
}

__noreturn void StP_PanicFromContext(StStatus status, uintptr_t rbp, uintptr_t rip, const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    panic_common(status, rbp, rip, fmt, args);
    va_end(args);
}
