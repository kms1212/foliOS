    .org    0

    .section .text
    .code32
    
    .macro  DEFINE_ISR num error
    .globl  _pc_isr_\num
_pc_isr_\num:
    cld

    .if     \error
    .else
    pushl   $0
    .endif

    pushal                              # push all registers
    xor     %eax, %eax
    mov     %ds, %ax
    push    %eax
    mov     %es, %ax
    push    %eax
    mov     %fs, %ax
    push    %eax
    mov     %gs, %ax
    push    %eax

    push    %ebp                        # set up stack base pointer
    mov     %esp, %ebp

    pushl   $0x\num                     # interrupt num
    lea     4(%ebp), %eax               # registers
    pushl   %eax
    lea     52(%ebp), %eax              # interrupt frame
    pushl   %eax
    
    call    _pc_isr_common
    add     $12, %esp

    pop     %ebp                        # restore base pointer
    pop     %eax
    mov     %ax, %gs                    # pop all registers
    pop     %eax
    mov     %ax, %fs
    pop     %eax
    mov     %ax, %es
    pop     %eax
    mov     %ax, %ds
    popal

    add     $4, %esp                    # remove error code

    iret                                # return from interrupt
    .endm

    DEFINE_ISR      00, 0
    DEFINE_ISR      01, 0
    DEFINE_ISR      02, 0
    DEFINE_ISR      03, 0
    DEFINE_ISR      04, 0
    DEFINE_ISR      05, 0
    DEFINE_ISR      06, 0
    DEFINE_ISR      07, 0
    DEFINE_ISR      08, 0
    DEFINE_ISR      09, 0
    DEFINE_ISR      0a, 1
    DEFINE_ISR      0b, 1
    DEFINE_ISR      0c, 1
    DEFINE_ISR      0d, 1
    DEFINE_ISR      0e, 1
    DEFINE_ISR      0f, 0
    DEFINE_ISR      10, 0
    DEFINE_ISR      11, 0
    DEFINE_ISR      12, 0
    DEFINE_ISR      13, 0
    DEFINE_ISR      14, 0
    DEFINE_ISR      15, 1
    DEFINE_ISR      16, 0
    DEFINE_ISR      17, 0
    DEFINE_ISR      18, 0
    DEFINE_ISR      19, 0
    DEFINE_ISR      1a, 0
    DEFINE_ISR      1b, 0
    DEFINE_ISR      1c, 0
    DEFINE_ISR      1d, 1
    DEFINE_ISR      1e, 1
    DEFINE_ISR      1f, 0

    .irpc   x,23456789abcdef
    .irpc   y,0123456789abcdef
    DEFINE_ISR      \x\y, 0
    .endr
    .endr
