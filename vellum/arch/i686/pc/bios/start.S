    .org    0

    .section .text.startup
    .code32
    .globl  _start
_start:
    mov     $0x98000, %esp

    xor     %eax, %eax
    mov     $__bss_start, %edi
    mov     $__bss_end, %ecx
    sub     $__bss_start, %ecx
    rep     stosb

.jump:
    xor     %ebp, %ebp
    call    _pc_init                    # jump to c code.
                                        # this function should not return

.halt:
    hlt
    jmp     .halt
