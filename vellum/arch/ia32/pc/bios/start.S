    .org    0

    .section .text.startup
    .code32
    .globl  _start
_start:
    mov     $0x98000, %esp

    xor     %eax, %eax
    mov     $_bss_start_, %edi
    mov     $_bss_end_, %ecx
    sub     $_bss_start_, %ecx
    rep     stosb

.jump:
    xor     %ebp, %ebp
    call    _pc_init                    # jump to c code.
                                        # this function should not return

.halt:
    hlt
    jmp     .halt
