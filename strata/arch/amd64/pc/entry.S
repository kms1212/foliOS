    .org    0

    .section .trampoline, "ax", @progbits
    .incbin "trampoline.bin"

    .section .text.startup
    .code32
.start64_addr:
    .quad   .start64

    .globl  _start
_start:
    cld
    cli

    mov     $__trampoline_load, %edi    # copy .trampoline
    mov     $__trampoline_start, %esi
    mov     $__trampoline_end, %ecx
    sub     %esi, %ecx
    add     $3, %ecx
    shr     $2, %ecx
    rep     movsl

    xor     %ebp, %ebp                  # sanitize stack

    and     $0xFFFFFFF0, %esp
    pushl   $0                          # push bootinfo (8 byte padding)
    push    %edx
    pushl   .start64_addr + 4           # push return address
    pushl   .start64_addr
    mov     $__trampoline_load, %eax
    call    *%eax

0:
    hlt
    jmp     0b

    .text
    .code64
.start64:                               # start 64bit kernel
    mov     $0x10, %ax
    mov     %ax, %ds
    mov     %ax, %es
    mov     %ax, %fs
    mov     %ax, %gs
    mov     %ax, %ss

    cld
    cli

    /* BSS Clear */
    xor     %rax, %rax
    leaq    __bss_start, %rdi
    leaq    __bss_end, %rcx
    sub     %rdi, %rcx
    add     $7, %rcx
    shr     $3, %rcx
    rep     stosq

    lea     _early_stack, %rsp         # init stack
    xor     %ebp, %ebp

    mov     %rdx, %rdi                  # bootinfo
    call    _pc_init

    call    main

.halt:
    hlt
    jmp     .halt

    .bss
    .align  4096

    .globl  _early_stack
    .skip   16384
_early_stack:

