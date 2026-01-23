    .org    0
    
    .section .text
    .code32
    .globl  setjmp
setjmp:
    mov     4(%esp), %ecx               # jmp_buf
    mov     (%esp), %edx                # return addr

    mov     %edx, (%ecx)
    mov     %ebx, 4(%ecx)
    mov     %esp, 8(%ecx)
    mov     %ebp, 12(%ecx)
    mov     %esi, 16(%ecx)
    mov     %edi, 20(%ecx)
    mov     %eax, 24(%ecx)

    xor     %eax, %eax

    ret

    .globl  longjmp
longjmp:
    mov     4(%esp), %ecx               # jmp_buf

    mov     (%ecx), %edx
    mov     %edx, (%esp)                # set return addr

    mov     4(%ecx), %ebx
    mov     8(%ecx), %esp
    mov     12(%ecx), %ebp
    mov     16(%ecx), %esi
    mov     20(%ecx), %edi
    mov     24(%ecx), %eax

    test    %eax, %eax
    jnz     1f
    inc     %eax

1:
    ret
