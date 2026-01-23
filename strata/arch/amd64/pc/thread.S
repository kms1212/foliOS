    .org    0

    .section .text
    .code64
    .globl  _StThreadP_RealThreadEntry
_StThreadP_RealThreadEntry:
    cld

    xor     %rbp, %rbp
    push    %rbp
    mov     %rsp, %rbp

    call    *%rbx

    call    StThread_Exit
