    .org    0

    .section .text
    .code32
    .globl  _StThreadP_KernelThreadEntry
_StThreadP_KernelThreadEntry:
    cld

    xor     %ebp, %ebp
    push    %ebp
    mov     %esp, %ebp

    push    %ebx
    call    *%ecx
    add     $4, %esp

    call    StThread_Exit
