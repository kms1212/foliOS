    .org    0

    .section .text.startup
    .code32
    .globl  _start
_start:
    push    %ebp
    mov     %esp, %ebp

    xor     %eax, %eax                  # clear trampoline bss
    mov     $_bss_start_, %edi 
    mov     $_bss_end_, %ecx
    sub     %edi, %ecx
    add     $3, %ecx
    shr     $2, %ecx
    rep     stosl

    call    setup_trampoline_page_tables # migrate page table

    call    get_trampoline_pml4_phys    # get page table physical address
    mov     %eax, %ebx

    mov     %cr0, %eax                  # disable paging
    and     $0x7FFFFFFF, %eax
    mov     %eax, %cr0

    mov     %ebx, %eax                  # load CR3
    shl     $12, %eax
    movl    %eax, %cr3

    mov     %cr4, %eax                  # enable PAE
    or      $0x00000020, %eax
    mov     %eax, %cr4

    mov     $0xC0000080, %ecx           # set EFER MSR
    rdmsr
    or      $0x00000100, %eax           # LME bit
    wrmsr

    mov     %cr0, %eax                  # enable paging
    or      $0x80000000, %eax
    mov     %eax, %cr0

    lgdt    (.temp_gdt_ptr)             # load gdt and jump
    ljmp    $0x08, $.start64

    .code64
.start64:
    mov     $0x10, %ax
    mov     %ax, %ds
    mov     %ax, %es
    mov     %ax, %fs
    mov     %ax, %gs
    mov     %ax, %ss

    mov     16(%rbp), %rdx
    mov     8(%rbp), %rax                        # jump to requested address
    jmp     *%rax

    .section .data
    .align 16
.temp_gdt:
    .quad 0x0000000000000000
    .quad 0x00209A0000000000        # Code (Ring 0)
    .quad 0x0000920000000000        # Data (Ring 0)
.temp_gdt_end:

.temp_gdt_ptr:
    .word .temp_gdt_end - .temp_gdt - 1
    .long .temp_gdt
