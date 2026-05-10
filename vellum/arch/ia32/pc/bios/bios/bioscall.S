    .org    0

    .globl  _pc_remap_pic_int
    
    .section .text
    .code32
    .globl  VlBiosP_Call
VlBiosP_Call:
    push    %ebp                        # set up stack base pointer
    mov     %esp, %ebp

    pushal                              # push registers
    pushfl
    cli

    mov     8(%ebp), %al                # arguments start from offset +8
    mov     %al, .Lirq                  # load irq number
    mov     12(%ebp), %edx              # load stuct pointer

    push    %ebp                        # store base pointer for later use

    mov     (%edx), %eax                 # load registers
    mov     %eax, .Lax
    mov     4(%edx), %eax
    mov     %eax, .Lbx
    mov     8(%edx), %eax
    mov     %eax, .Lcx
    mov     12(%edx), %eax
    mov     %eax, .Ldx
    mov     16(%edx), %eax
    mov     %eax, .Lsi
    mov     20(%edx), %eax
    mov     %eax, .Ldi
    mov     24(%edx), %eax
    mov     %eax, .Lbp
    mov     28(%edx), %ax
    mov     %ax, .Lds
    mov     30(%edx), %ax
    mov     %ax, .Les

    call    switch_p32_to_r16         # switch to real mode
    .code16

    clc

    .byte   0x66, 0xB8                  # mov imm32, %eax
.Lbx:
    .long   0
    mov     %eax, %ebx                  # move to proper register

    .byte   0x66, 0xB8                  # mov imm32, %eax
.Lcx:
    .long   0
    mov     %eax, %ecx

    .byte   0x66, 0xB8                  # mov imm32, %eax
.Ldx:
    .long   0
    mov     %eax, %edx

    .byte   0x66, 0xB8                  # mov imm32, %eax
.Lsi:
    .long   0
    mov     %eax, %esi

    .byte   0x66, 0xB8                  # mov imm32, %eax
.Ldi:
    .long   0
    mov     %eax, %edi

    .byte   0x66, 0xB8                  # mov imm32, %eax
.Lbp:
    .long   0
    mov     %eax, %ebp

    .byte   0xB8                        # mov imm16, %ax
.Les:
    .word   0
    mov     %ax, %es

    .byte   0xB8                        # mov imm16, %ax
.Lds:
    .word   0
    mov     %ax, %ds

    .byte   0x66, 0xB8                  # mov imm32, %eax
.Lax:
    .long   0

    .byte   0xCD                        # int imm8
.Lirq:
.Lerr:
    .byte   0

    push    %ax                         # clear DS
    xor     %ax, %ax
    mov     %ax, %ds
    pop     %ax

    setc    .Lerr                       # store carry flag

    mov     %eax, .Lax                   # store registers temporarily
    mov     %ebx, .Lbx
    mov     %ecx, .Lcx
    mov     %edx, .Ldx
    mov     %esi, .Lsi
    mov     %edi, .Ldi
    mov     %ebp, .Lbp
    mov     %es, %ax
    mov     %ax, .Les
    mov     %ds, %ax
    mov     %ax, .Lds

    call    switch_r16_to_p32         # switch back to protected mode

    .code32 
    pop     %ebp                        # restore stack base pointer

    mov     12(%ebp), %edx              # load stuct pointer

    mov     .Lax, %eax                   # store registers
    mov     %eax, (%edx)
    mov     .Lbx, %eax
    mov     %eax, 4(%edx)
    mov     .Lcx, %eax      
    mov     %eax, 8(%edx)
    mov     .Ldx, %eax
    mov     %eax, 12(%edx)
    mov     .Lsi, %eax
    mov     %eax, 16(%edx)
    mov     .Ldi, %eax
    mov     %eax, 20(%edx)
    mov     .Lbp, %eax
    mov     %eax, 24(%edx)
    mov     .Lds, %ax
    mov     %ax, 28(%edx)
    mov     .Les, %ax
    mov     %ax, 30(%edx)

    popfl
    popal                               # pop registers

    xor     %eax, %eax
    movb    .Lerr, %al

    pop     %ebp                        # restore base pointer

    ret

    .globl  VlBiosP_CallWithInterrupts
VlBiosP_CallWithInterrupts:
    push    %ebp
    mov     %esp, %ebp

    pushal
    pushfl
    cli

    mov     8(%ebp), %al
    mov     %al, .Lirq_int
    mov     12(%ebp), %edx

    push    %ebp

    mov     (%edx), %eax
    mov     %eax, .Lax_int
    mov     4(%edx), %eax
    mov     %eax, .Lbx_int
    mov     8(%edx), %eax
    mov     %eax, .Lcx_int
    mov     12(%edx), %eax
    mov     %eax, .Ldx_int
    mov     16(%edx), %eax
    mov     %eax, .Lsi_int
    mov     20(%edx), %eax
    mov     %eax, .Ldi_int
    mov     24(%edx), %eax
    mov     %eax, .Lbp_int
    mov     28(%edx), %ax
    mov     %ax, .Lds_int
    mov     30(%edx), %ax
    mov     %ax, .Les_int

    call    switch_p32_to_r16
    .code16

    clc

    .byte   0x66, 0xB8
.Lbx_int:
    .long   0
    mov     %eax, %ebx

    .byte   0x66, 0xB8
.Lcx_int:
    .long   0
    mov     %eax, %ecx

    .byte   0x66, 0xB8
.Ldx_int:
    .long   0
    mov     %eax, %edx

    .byte   0x66, 0xB8
.Lsi_int:
    .long   0
    mov     %eax, %esi

    .byte   0x66, 0xB8
.Ldi_int:
    .long   0
    mov     %eax, %edi

    .byte   0x66, 0xB8
.Lbp_int:
    .long   0
    mov     %eax, %ebp

    .byte   0xB8
.Les_int:
    .word   0
    mov     %ax, %es

    .byte   0xB8
.Lds_int:
    .word   0
    mov     %ax, %ds

    .byte   0x66, 0xB8
.Lax_int:
    .long   0

    sti
    .byte   0xCD
.Lirq_int:
.Lerr_int:
    .byte   0
    cli

    push    %ax
    xor     %ax, %ax
    mov     %ax, %ds
    pop     %ax

    setc    .Lerr_int

    mov     %eax, .Lax_int
    mov     %ebx, .Lbx_int
    mov     %ecx, .Lcx_int
    mov     %edx, .Ldx_int
    mov     %esi, .Lsi_int
    mov     %edi, .Ldi_int
    mov     %ebp, .Lbp_int
    mov     %es, %ax
    mov     %ax, .Les_int
    mov     %ds, %ax
    mov     %ax, .Lds_int

    call    switch_r16_to_p32

    .code32
    pop     %ebp

    mov     12(%ebp), %edx

    mov     .Lax_int, %eax
    mov     %eax, (%edx)
    mov     .Lbx_int, %eax
    mov     %eax, 4(%edx)
    mov     .Lcx_int, %eax
    mov     %eax, 8(%edx)
    mov     .Ldx_int, %eax
    mov     %eax, 12(%edx)
    mov     .Lsi_int, %eax
    mov     %eax, 16(%edx)
    mov     .Ldi_int, %eax
    mov     %eax, 20(%edx)
    mov     .Lbp_int, %eax
    mov     %eax, 24(%edx)
    mov     .Lds_int, %ax
    mov     %ax, 28(%edx)
    mov     .Les_int, %ax
    mov     %ax, 30(%edx)

    popfl
    popal

    xor     %eax, %eax
    movb    .Lerr_int, %al

    pop     %ebp

    ret
