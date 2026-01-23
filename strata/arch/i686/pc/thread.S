    .org    0

    .section .text
    .code32
    .globl  _StThreadP_RealThreadEntry
_StThreadP_RealThreadEntry:
    cld

    xor     %ebp, %ebp
    push    %ebp
    mov     %esp, %ebp

    push    %ebx
    call    *%ecx
    add     $4, %esp

    call    StThread_Exit
