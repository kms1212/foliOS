    .org    0

    .set    REAL_STACK_INIT, 0x00007BFC  # right below the inital code area
    .set    PROT_STACK_INIT, 0x00007000

    .lcomm  cr0_initialized, 2
    .lcomm  cr3_initialized, 2
    .lcomm  cr0, 4
    .lcomm  cr3, 4

    .section .text
    .code16
    .globl  switch_r16_to_p32
switch_r16_to_p32:
    cli                                 # clear interrupt

    xor     %ax, %ax
    mov     %ax, %ds                    # clear ds

    lgdt    _i686_gdtr                  # load gdt

    mov     cr3_initialized, %ax
    test    %ax, %ax
    jz      .skip_cr3_load
    mov     cr3, %eax
    mov     %eax, %cr3
.skip_cr3_load:

    mov     cr0_initialized, %ax
    test    %ax, %ax
    jnz     .skip_cr0_load
    mov     %cr0, %eax
    or      $1, %eax                    # enable protected mode
    mov     %eax, cr0
    movw    $1, cr0_initialized

.skip_cr0_load:
    mov     cr0, %eax                   # get cr0
    mov     %eax, %cr0                  # set cr0

    pushw   _i686_pm32_code_seg
    pushw   $.pmode
    retf                                # switch code segment
                                        # the .code32 directive should be placed
                                        # after this line of code

    .code32
.pmode:

    mov     _i686_pm32_data_seg, %ax
    mov     %ax, %ds
    mov     %ax, %es
    mov     %ax, %fs
    mov     %ax, %gs
    mov     %ax, %ss                    # set segments

    lidt    _i686_idtr                  # load idt

    xor     %eax, %eax
    mov     (%esp), %ax
    mov     %ax, REAL_STACK_INIT        # save current return address

    mov     prot_stack, %eax
    mov     %eax, %esp
    mov     %eax, %ebp                  # restore protected mode stack

    xor     %eax, %eax
    mov     REAL_STACK_INIT, %ax
    mov     %eax, (%esp)                # load current return address

    xor     %eax, %eax                  # clear eax

    ret

    .globl  switch_p32_to_r16
switch_p32_to_r16:
    cli                                 # clear interrupt
    lgdt    _i686_gdtr                  # load gdt

    mov     %cr0, %eax                  # save cr0
    mov     %eax, cr0

    mov     %cr3, %eax                  # save cr3
    mov     %eax, cr3
    movw    $1, cr3_initialized

    pushl   $0
    pushw   $1023
    lidt    (%esp)
    add     $6, %esp                    # load ivt

    mov     %esp, %eax
    mov     %eax, prot_stack            # save stack

    mov     (%esp), %eax
    mov     %eax, REAL_STACK_INIT       # real mode stack is not preserved

    mov     $REAL_STACK_INIT, %eax
    mov     %eax, %esp
    mov     %eax, %ebp                  # set up stack pointer, base pointer

    pushw   _i686_pm16_code_seg
    pushl   $.pmode16
    ljmp    *(%esp)                     # switch code segment
                                        # the .code16 directive should be placed
                                        # after this line of code

    .code16
.pmode16:
    add     $6, %esp

    mov     _i686_pm16_data_seg, %ax
    mov     %ax, %ds
    mov     %ax, %es
    mov     %ax, %fs
    mov     %ax, %gs
    mov     %ax, %ss                    # set up segments

    mov     %cr0, %eax                  # get cr0
    and     $~0x80000001, %eax          # disable paging & protected mode
    mov     %eax, %cr0                  # set cr0

    xor     %eax, %eax                  # clear cr3
    mov     %eax, %cr3

    ljmp    $0, $.rmode                 # reload code segment
                                        # assuming that we're in 0-FFFFh area

.rmode:                                 # real mode
    xor     %eax, %eax
    mov     %ax, %ds
    mov     %ax, %es
    mov     %ax, %fs
    mov     %ax, %gs
    mov     %ax, %ss                    # set up segments

    ret


    .section .data
prot_stack:                             # protected mode stack
    .long   PROT_STACK_INIT

    .align  8
    .word   0
    .globl  _i686_idtr
_i686_idtr:
    .word   1023
    .long   0

    .align  8
    .word   0
    .globl _i686_gdtr
_i686_gdtr:
    .word   8 * 5 - 1                   # 5 entries
    .long   _i686_default_gdt           # gdt pointer
    
    .globl _i686_pm32_code_seg
_i686_pm32_code_seg:
    .word   0x0008

    .globl _i686_pm32_data_seg
_i686_pm32_data_seg:
    .word   0x0010

    .globl _i686_pm16_code_seg
_i686_pm16_code_seg:
    .word   0x0018

    .globl _i686_pm16_data_seg
_i686_pm16_data_seg:
    .word   0x0020

    .section .rodata
    .globl _i686_default_gdt
_i686_default_gdt:
    .word   0x0000                      # null descriptor
    .word   0x0000
    .byte   0x00
    .byte   0x00
    .byte   0x00
    .byte   0x00
    
    .word   0xFFFF                      # code segment
    .word   0x0000
    .byte   0x00
    .byte   0x9A
    .byte   0xCF
    .byte   0x00
        
    .word   0xFFFF                      # data segment
    .word   0x0000
    .byte   0x00
    .byte   0x92
    .byte   0xCF
    .byte   0x00
    
    .word   0xFFFF                      # 16-bit protected mode code segment
    .word   0x0000
    .byte   0x00
    .byte   0x9A
    .byte   0x00
    .byte   0x00

    .word   0xFFFF                      # 16-bit protected mode data segment
    .word   0x0000
    .byte   0x00
    .byte   0x92
    .byte   0x00
    .byte   0x00
