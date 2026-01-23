    .org    0
    
    .section .text
    .code32
    .globl  _pc_vbe_pmi_set_display_start
_pc_vbe_pmi_set_display_start:
    push    %ebp
    mov     %esp, %ebp

    push    %bx
    push    %cx
    push    %dx
    push    %es

    xor     %eax, %eax
    mov     8(%ebp), %ax
    shl     $4, %eax
    add     10(%ebp), %eax
    add     $2, %ax

    xor     %bx, %bx
    mov     12(%ebp), %cx
    mov     14(%ebp), %dx

    jmp     *%eax

    mov     %ax, %bx
    xor     %eax, %eax
    mov     %bx, %ax

    add     $4, %esp
    pop     %es
    pop     %dx
    pop     %cx
    pop     %bx
    pop     %ebp
    ret
