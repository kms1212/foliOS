    .org    0

    .set    STACK_START_VPN,0x000FFBC0
    .set    STACK_POINTER,  0xFFC00000

    .section .text.startup
    .code32
    .globl  _start
_start:
    cld
    cli

    xor     %eax, %eax
    mov     $___bss_start, %edi          # clear bss
    mov     $___bss_end, %ecx
    sub     %edi, %ecx
    add     $3, %ecx
    shr     $2, %ecx
    rep     stosl

    xor     %ebp, %ebp                  # reset stack chain

    push    %edx                        # call init
    call    _pc_init
    add     $4, %esp

    pushl   $64                         # allocate stack area
    pushl   $STACK_START_VPN
    call    mm_allocate_pages_to        # TODO: check return status
    add     $8, %esp

    mov     0xFFFFF000, %eax            # unmark page table of page directory #0
    shr     $12, %eax
    push    %eax
    push    %eax
    call    mm_pma_unmark_reserved      # TODO: check return status
    add     $8, %esp

    movl    $0, 0xFFFFF000              # unmap page directory #0

    mov     %cr3, %eax                  # flush tlb
    mov     %eax, %cr3

    mov     $STACK_POINTER, %esp          # set new stack

    call    _pc_init_late               # late init

    mov     $__init_array_start, %ebx   # run init array
0:
    cmp     $__init_array_end, %ebx
    je      1f
    call    *(%ebx)
    jmp     0b
1:

    sti

.jump:
    call    main                        # jump to c code.
                                        # this function should not return

.halt:
    hlt
    jmp     .halt
