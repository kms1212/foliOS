    .org    0

#include <strata/plat/gdt_constants.h>

    .section .text
    .code64
    .globl  _StThreadP_KernelThreadEntry
_StThreadP_KernelThreadEntry:
    cld

    xor     %rbp, %rbp                  # setup stack frame
    push    %rbp
    mov     %rsp, %rbp

    sub     $8, %rsp                    # align stack pointer

    call    *%rbx                       # call thread entry

    call    StThread_Exit

    .globl  _StThreadP_UserThreadEntry
_StThreadP_UserThreadEntry:
    cld

    mov     $SEG_SEL_USER_DATA | 3, %rax # setup user data segment
    mov     %ax, %ds
    mov     %ax, %es
    mov     %ax, %fs

    swapgs                              # switch gs
    mov     %ax, %gs

    push    %rax                        # push fake interrupt frame
    push    %rdi
    pushq   $0x0000000000000202
    pushq   $SEG_SEL_USER_CODE | 3
    push    %rbx

    xor     %rax, %rax                  # clear registers
    xor     %rbx, %rbx
    xor     %rcx, %rcx
    xor     %rdx, %rdx
    xor     %rbp, %rbp
    xor     %rsi, %rsi
    xor     %rdi, %rdi
    xor     %r8, %r8
    xor     %r9, %r9
    xor     %r10, %r10
    xor     %r11, %r11
    xor     %r12, %r12
    xor     %r13, %r13
    xor     %r14, %r14
    xor     %r15, %r15

    iretq

    .globl  StThreadP_Yield
StThreadP_Yield:
    pop     %rax                        # get return address
    mov     %rsp, %r10

    pushfq
    pop     %r11

    cli

    pushq   $SEG_SEL_KERNEL_DATA        # push fake interrupt frame
    push    %r10
    push    %r11
    pushq   $SEG_SEL_KERNEL_CODE
    push    %rax
    pushq   $0

    push    %rax                        # push all registers
    push    %rbx
    push    %rcx
    push    %rdx
    push    %rbp
    push    %rsi
    push    %rdi
    push    %r8
    push    %r9
    push    %r10
    push    %r11
    push    %r12
    push    %r13
    push    %r14
    push    %r15

    sub     $8, %rsp                    # align stack pointer

    mov     %rsp, %rbp                  # save stack base pointer

    lea     8(%rbp), %rdi               # registers

    call    _StThreadP_DoYield

    test    %rax, %rax                  # do not switch stack if yield function
    jz      0f                          # returned NULL
    mov     %rax, %rsp                  # switch stack
0:

    testb   $3, 144(%rsp)               # check if we should jump to user mode
    jz      0f
    swapgs
0:

    add     $8, %rsp                    # skip stack pointer padding

    pop     %r15                        # pop all registers
    pop     %r14
    pop     %r13
    pop     %r12
    pop     %r11
    pop     %r10
    pop     %r9
    pop     %r8
    pop     %rdi
    pop     %rsi
    pop     %rbp
    pop     %rdx
    pop     %rcx
    pop     %rbx
    pop     %rax

    add     $8, %rsp                    # remove error code

    iretq
