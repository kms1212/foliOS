    .org    0

    .globl  _pc_pic_remap_int
    
    .section .text
    .code32
    .globl  _pc_bios_bootnext
_pc_bios_bootnext:
    push    %ebp
    mov     %esp, %ebp

    cli

    mov     $0x60, %al              # enable keyboard translation
    outb    %al, $0x0064
    mov     $0x63, %al
    outb    %al, $0x0060

    pushl   $0x70                   # remap PIC
    pushl   $0x08
    call    _pc_pic_remap_int
    add     $8, %esp

    mov     8(%ebp), %dl            # get drive number

    call    switch_p32_to_r16

    .code16
    xor     %ax, %ax                # Set video mode 0x03
    mov     $0x03, %al
    int     $0x10

    in      $0x92, %al              # Disable A20
    and     $0xFD, %al
    out     %al, $0x92

    int     $0x18
