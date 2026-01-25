    .org    0

    .section .text
    .code64
    
    .macro  DEFINE_ISR num error
    .globl  _pc_isr_\num
_pc_isr_\num:
    cld

    .if     \error
    .else
    pushq   $0
    .endif

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

    # TODO: load kernel DS/ES

    sub     $8, %rsp                    # align stack pointer

    mov     %rsp, %rbp                  # set up stack base pointer

    testb   $3, 144(%rbp)
    jz      0f
    swapgs
0:

    mov     $0x\num, %rdx               # interrupt num
    lea     8(%rbp), %rsi               # registers
    lea     128(%rbp), %rdi             # interrupt frame + error

    call    _pc_isr_common

    test    %rax, %rax                  # do not switch stack if common isr
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

    iretq                               # return from interrupt
    .endm

    DEFINE_ISR      00, 0
    DEFINE_ISR      01, 0
    DEFINE_ISR      02, 0
    DEFINE_ISR      03, 0
    DEFINE_ISR      04, 0
    DEFINE_ISR      05, 0
    DEFINE_ISR      06, 0
    DEFINE_ISR      07, 0
    DEFINE_ISR      08, 1
    DEFINE_ISR      09, 0
    DEFINE_ISR      0a, 1
    DEFINE_ISR      0b, 1
    DEFINE_ISR      0c, 1
    DEFINE_ISR      0d, 1
    DEFINE_ISR      0e, 1
    DEFINE_ISR      0f, 0
    DEFINE_ISR      10, 0
    DEFINE_ISR      11, 1
    DEFINE_ISR      12, 0
    DEFINE_ISR      13, 0
    DEFINE_ISR      14, 0
    DEFINE_ISR      15, 1
    DEFINE_ISR      16, 0
    DEFINE_ISR      17, 1
    DEFINE_ISR      18, 0
    DEFINE_ISR      19, 0
    DEFINE_ISR      1a, 0
    DEFINE_ISR      1b, 0
    DEFINE_ISR      1c, 0
    DEFINE_ISR      1d, 1
    DEFINE_ISR      1e, 1
    DEFINE_ISR      1f, 0

    .irpc   x,2345679abcdef
    .irpc   y,0123456789abcdef
    DEFINE_ISR      \x\y, 0
    .endr
    .endr

    DEFINE_ISR      81, 0
    DEFINE_ISR      82, 0
    DEFINE_ISR      83, 0
    DEFINE_ISR      84, 0
    DEFINE_ISR      85, 0
    DEFINE_ISR      86, 0
    DEFINE_ISR      87, 0
    DEFINE_ISR      88, 1
    DEFINE_ISR      89, 0
    DEFINE_ISR      8a, 1
    DEFINE_ISR      8b, 1
    DEFINE_ISR      8c, 1
    DEFINE_ISR      8d, 1
    DEFINE_ISR      8e, 1
    DEFINE_ISR      8f, 0

    .globl  _pc_isr_80
_pc_isr_80:
    swapgs

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

    # TODO: load kernel DS/ES

    sub     $8, %rsp                    # align stack pointer

    mov     %rsp, %rbp                  # set up stack base pointer

    sti

    lea     8(%rbp), %rsi               # registers
    lea     128(%rbp), %rdi             # interrupt frame + error

    call    StSyscallP_Handler          # call syscall handler

    cli

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

    swapgs

    iretq                               # return from interrupt
