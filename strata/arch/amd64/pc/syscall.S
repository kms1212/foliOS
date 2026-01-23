    .org    0

#include <strata/plat/gdt_constants.h>

    .section .text
    .code64
    .globl  _StSyscallP_Entry
_StSyscallP_Entry:
    swapgs
    mov     %rsp, %gs:0x08
    mov     %gs:0x00, %rsp

    pushq   $SEG_SEL_USER_DATA          # make fake interrupt frame
    pushq   %gs:0x08
    push    %r11
    pushq   $SEG_SEL_USER_CODE
    push    %rcx
    pushq   $0                          # error code

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

    mov     %r10, %rcx                  # convert user abi to syscall abi
    mov     %rsp, %rbp                  # set up stack base pointer

    sti

    mov     %rsp, %rsi                  # pass registers
    lea     120(%rsp), %rdi             # pass interrupt stack frame
    call    _StSyscallP_Handler         # call syscall handler

    cli

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

    add     $8, %rsp                    # skip error code

    pop     %rcx
    add     $8, %rsp
    pop     %r11
    pop     %rsp

    swapgs
    sysretq
