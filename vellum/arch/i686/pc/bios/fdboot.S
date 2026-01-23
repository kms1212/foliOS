    .set    MAGIC,          0xAA55
    .set    STACK_INIT,     0x7000
    .set    ROOTDIR_BUFFER, 0x7E00
    .set    FAT_BUFFER,     0x7000
    .set    DATA_BUFFER,    0x8000
    .set    LOAD_SEGMENT,   0x0900
    .set    ENTRY_ADDR,     0x9000

    .org    0

    .lcomm .fat_lba, 4
    .lcomm .root_dir_lba, 4
    .lcomm .cluster_start_lba, 4

    .section .text.startup
    .code16
    .globl  _start
_start:
    jmp     .start1                     # EB 3C
    nop                                 # 90

.bpb_oem_name:
    .ascii  "        "

.bpb_bytes_per_sector:
    .word   0x0000

.bpb_sectors_per_cluster:
    .byte   0x00

.bpb_reserved_sectors:
    .word   0x0000

.bpb_fat_count:
    .byte   0x00

.bpb_root_entries:
    .word   0x0000

.bpb_total_sectors_16:
    .word   0x0000

.bpb_media_type:
    .byte   0x00

.bpb_sectors_per_fat:
    .word   0x0000

.bpb_sectors_per_track:
    .word   0x0000

.bpb_head_count:
    .word   0x0000

.bpb_hidden_sectors:
    .long   0x00000000

.bpb_total_sectors_32:
    .long   0x00000000

.bpb_drive_number:
    .byte   0x00

.bpb_reserved1:
    .byte   0x00

.bpb_boot_signature:
    .byte   0x00

.bpb_volume_id:
    .long   0x00000000

.bpb_volume_label:
    .ascii  "           "

.bpb_file_system_type:
    .ascii  "FAT12   "
    
.start1:
    xor     %ax, %ax                    # clear segment registers
    mov     %ax, %ds
    mov     %ax, %es
    mov     %ax, %ss
    mov     $STACK_INIT, %sp            # setup stack pointer
    mov     %sp, %bp

.get_drive_geometry:
    push    %dx
    mov     %dl, .bpb_drive_number

    mov     $0x08, %ah                  # BIOS Int 13h, AH=0x08: Get Drive Geometry
    int     $0x13                       # do it!
    jc      print_error

    xor     %ax, %ax
    mov     %ax, %es

    mov     %dh, %dl
    xor     %dh, %dh
    inc     %dx
    mov     %dx, .bpb_head_count

    and     $0x3F, %cx
    mov     %cx, .bpb_sectors_per_track

    pop     %dx

.find_loader:
    # find BOOTLDR.BIN from the root directory

    # calc FAT lba
    xor     %eax, %eax
    mov     .bpb_reserved_sectors, %ax
    mov     %eax, .fat_lba

    # calc root directory lba
    xor     %eax, %eax
    xor     %cx, %cx
    mov     .bpb_sectors_per_fat, %ax
    mov     .bpb_fat_count, %cl
    mul     %cx
    add     .bpb_reserved_sectors, %ax
    mov     %eax, .root_dir_lba

    # calc cluster start lba
    xor     %eax, %eax
    mov     .bpb_root_entries, %ax
    mov     $32, %bx
    mulw    %bx
    add     $0x200, %ax
    dec     %ax
    mov     $0x200, %bx
    divw    %bx
    add     .root_dir_lba, %eax
    mov     %eax, .cluster_start_lba

    # read the first sector of the root directory
    mov     $0x0001, %ax                # read 1 sector
    mov     $ROOTDIR_BUFFER, %bx        # to buffer starting at 0x7E00
    mov     .root_dir_lba, %cx
    call    read_disk

    mov     $ROOTDIR_BUFFER, %si
.compare_filename:
    mov     $str_bootfilename, %di      # compare file name
    push    %si
    push    %di
    cmpw    $0x00, (%si)
    jz      .compare_filename.notfound
    mov     $11, %cx
    repe    cmpsb
    je      .compare_filename.found

    pop     %di
    pop     %si
    add     $0x20, %si
    jmp     .compare_filename           # next entry

.compare_filename.notfound:
    mov     $str_bootfile_not_found, %si
    jmp     print_error

.compare_filename.found:
    pop     %di
    pop     %si

.load_fat:
    mov     .fat_lba, %ecx
    mov     $FAT_BUFFER, %bx
    mov     $1, %ax
    call    read_disk

.load_file:
    # read first cluster
    xor     %di, %di
    mov     26(%si), %dx                # dx = cluster to read

.load_file.read:
    push    %dx
    xor     %eax, %eax                  # clear es
    mov     %ax, %es

    mov     %dx, %ax                    # calc cluster to sector lba
    sub     $2, %ax
    xor     %bx, %bx
    mov     .bpb_sectors_per_cluster, %bl
    mulw    %bx
    add     .cluster_start_lba, %eax

    mov     %eax, %ecx                  # read cluster
    mov     $DATA_BUFFER, %bx
    mov     .bpb_sectors_per_cluster, %al
    call    read_disk

.load_file.copy:
    mov     $LOAD_SEGMENT, %ax          # copy buffer to data
    mov     %ax, %es
    mov     $DATA_BUFFER, %si
    xor     %cx, %cx
    mov     .bpb_sectors_per_cluster, %cl
    shl     $8, %cx
    rep     movsw

.load_file.next_cluster:
    pop     %dx

    mov     $0x0E, %ah
    xor     %bx, %bx
    mov     $'.', %al
    int     $0x10

    push    %dx
    mov     %dx, %ax
    shr     $1, %ax
    mov     $3, %bx
    mulw    %bx
    mov     $FAT_BUFFER, %cx
    mov     (%ecx, %eax, 1), %ebx
    pop     %dx
    test    $1, %dx
    jz      .load_file.next_cluster.skip_shift
    shr     $12, %ebx
.load_file.next_cluster.skip_shift:
    and     $0xFFF, %ebx

    cmp     $0x0FF8, %bx
    jge     .jump_next

    mov     %bx, %dx
    jmp     .load_file.read

.jump_next:
    xor     %edx, %edx
    mov     .bpb_drive_number, %dl
    mov     .bpb_hidden_sectors, %ecx

    jmp     ENTRY_ADDR


read_disk:
    # Read the disk using the BIOS interrupt call
    # Parameters:
    #   AL: number of sectors to read
    #   ECX: LBA
    #   DL: drive number
    #   ES:BX: pointer to buffer
    pusha

    push    %ax
    push    %bx

    add     .bpb_hidden_sectors, %ecx

    mov     %ecx, %eax
    and     $0x0000FFFF, %eax
    mov     %ecx, %edx
    shr     $16, %edx

    divw    .bpb_sectors_per_track       # temp = LBA / sectors_per_track
    inc     %dx
    mov     %dx, %bx                     # sector = (LBA % sectors_per_track) + 1

    xor     %dx, %dx
    divw    .bpb_head_count
                                         # head = temp % head_count
                                         # cylinder = temp / head_count

    xchg    %al, %ah
    shl     $6, %al
    or      %bl, %al

    mov     %ax, %cx

    mov     %dl, %dh
    mov     .bpb_drive_number, %dl

    pop     %bx
    pop     %ax
    mov     $0x02, %ah                  # BIOS Int 13h, AH=0x02: Read
    int     $0x13

    mov     $str_diskerror, %si
    jc      print_error

    popa
    ret

print_error:
    # Print error and halt
    # Parameters:
    #   DS:SI: error string
    mov     $0x0E, %ah
    xor     %bx, %bx                    # to page 0

print_error.1:
    lodsb
    or      %al, %al
    jz      halt                        # until the char is NULL

    int     $0x10                       # do it!
    jmp     print_error.1               # print next character

halt:
    hlt
    jmp     halt                        # halt

str_diskerror: 
    .asciz  "Disk read error\r\n"

str_bootfile_not_found:
    .asciz  "STAGE1.X86 not found\r\n"

str_bootfilename:
    .ascii  "STAGE1  X86"

    .org    0x1FE                       # 512 - 2
    .word   MAGIC                       # magic number
