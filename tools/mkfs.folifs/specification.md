# FOLIFS - A FileSystem

## First sector of reserved area

| Offset | Size | Value                              |
|--------|------|------------------------------------|
| 0      | 504  | Boot Code                          |
| 496    | 8    | Volume Offset                      |
| 504    | 4    | Filesystem Signature (asciz "FOLIFS") |
| 508    | 2    | Reserved Sectors                   |
| 510    | 2    | VBR Signature (0x55 0xAA)          |

## RDB - Root Descriptor Block

| Offset    | Size       | Value                |
|-----------|------------|----------------------|
| 0         | 8          | Total Sector Count   |
| 8         | 8          | Total Block Count    |
| 16        | 1          | Sectors per Block    |
| 17        | 1          | RDB Copy Count       |
| 18        | 2          | Bytes per Sector     |
| 20        | 4          | Flags                |
| 24        | 8          | Root MDB Pointer     |
| 32        | 8          | UDB Pointer          |
| 40        | 8          | JBB Pointer          |
| 48        | 8          | RBB Pointer          |
| 56        | 8          | Group 0 GBB Pointer  |
| 64        | 16         | Volume UUID          |
| 80        | 16         | Formatted OS (asciz) |
| 96        | 2          | Filesystem Version   |
| 98        | BLKSZ - 86 | Reserved             |
| BLKSZ - 4 | 4          | CRC32 Checksum       |

## RBB - Root Bitmap Block

| Offset    | Size      | Value          |
|-----------|-----------|----------------|
| 0         | BLKSZ - 4 | Bitmap         |
| BLKSZ - 4 | 4         | CRC32 Checksum |

## GBB - Group Bitmap Block

| Offset    | Size      | Value          |
|-----------|-----------|----------------|
| 0         | BLKSZ - 4 | Bitmap         |
| BLKSZ - 4 | 4         | CRC32 Checksum |

## MDB - MetaData Block

| Offset | Size | Value                           |
|--------|------|---------------------------------|
| 0      | 8    | Parent MDB Pointer              |
| 8      | 8    | Next MDB Pointer                |
| 16     | 4    | Flags                           |
| 20     | 4    | Attribute Entry Presence Bitmap |
| 24     | 8    | File Size                       |
| 32     | 8    | DHB or DDB Pointer              |
| 40     | 8    | Reserved                        |

### Attribute Entry Header

| Offset | Size | Value           |
|--------|------|-----------------|
| 0      | 4    | Entry Length    |
| 4      | 1    | Entry Type      |
| 5      | 3    | Reserved        |

### FAE - Filename Attribute Entry (Entry Type 1)

| Offset | Size | Value           |
|--------|------|-----------------|
| 0      | 4    | Filename Hash   |
| 4      | 4    | Reserved        |
| 8      | 1    | Filename Length |
| 9      | 255  | Filename        |

### TAE - Timestamp Attribute Entry (Entry Type 2)

| Offset | Size | Value                     |
|--------|------|---------------------------|
| 0      | 8    | Create Time / Format Time |
| 8      | 8    | Write Time                |
| 16     | 8    | Read Time / Mount Time    |

### PAE - Permission Attribute Entry (Entry Type 3)

| Offset | Size | Value           |
|--------|------|-----------------|
| 0      | 8    | ACB Pointer     |

### QAE - Quota Attribute Entry (Entry Type 4)

| Offset | Size | Value           |
|--------|------|-----------------|
| 0      | 8    | QCB Pointer     |

### EAE - Encryption Attribute Entry (Entry Type 5)

| Offset | Size | Value                |
|--------|------|----------------------|
| 0      | 1    | Encryption Algorithm |
| 1      | 15   | Reserved             |
| 16     | ?    | Decryption Test Data |

### CAE - Compression Attribute Entry (Entry Type 6)

| Offset | Size | Value                 |
|--------|------|-----------------------|
| 0      | 1    | Compression Algorithm |

## ACB - Access Control Block

| Offset        | Size | Value                   |
|---------------|------|-------------------------|
| 0             | 8    | Parent ACB Head Pointer |
| 8             | 8    | Next ACB Pointer        |
| 16            | 2    | Entry Count in this ACB |
| 18            | 6    | Reserved                |
| 24            | 32   | 1st ACB Entry           |
| 56            | 32   | 2nd ACB Entry           |
| 88            | 32   | 3rd ACB Entry           |
| ...           | 32   | Nth ACB Entry           |
| 32 * $N$ + 24 | 4    | Reserved                |
| 32 * $N$ + 28 | 4    | CRC32 Checksum          |

### ACB Entry

| Offset | Size | Value                 |
|--------|------|-----------------------|
| 0      | 16   | User / Group UUID     |
| 16     | 2    | Permission Bitmap     |
| 18     | 2    | Flags                 |
| 20     | 12   | Reserved              |

## QCB - Quota Control Block

| Offset        | Size  | Value                   |
|---------------|-------|-------------------------|
| 0             | 8     | Next QCB Pointer        |
| 8             | 2     | Entry Count in this QCB |
| 10            | 14    | Reserved                |
| 24            | 72    | 1st QCB Entry           |
| 96            | 72    | 2nd QCB Entry           |
| 168           | 72    | 3rd QCB Entry           |
| ...           | 72    | Nth QCB Entry           |
| 72 * $N$ + 24 | 28-84 | Reserved                |
| BLKSZ - 32    | 4     | CRC32 Checksum          |

$$ N = \lfloor\frac{(Block\ Size)}{72}\rfloor $$

### QCB Entry

| Offset | Size | Value                  |
|--------|------|----------------------- |
| 0      | 16   | User / Group UUID      |
| 16     | 8    | Current File Count     |
| 24     | 8    | File Count Limit       |
| 32     | 8    | Block Count Soft Limit |
| 40     | 8    | Block Count Hard Limit |
| 48     | 8    | Current Block Count    |
| 56     | 8    | Last Limit Alarm Time  |
| 64     | 4    | Flags                  |
| 68     | 4    | Reserved               |

## DHB - Directory Hash Block

| Offset        | Size | Value                   |
|---------------|------|-------------------------|
| 0             | 2    | Entry Count in this DHB |
| 2             | 14   | Resered                 |
| 16            | 48   | 1st DHB Entry           |
| 64            | 48   | 2nd DHB Entry           |
| 112           | 48   | 3rd DHB Entry           |
| ...           | 40   | $N$ th DHB Entry        |
| 48 * $N$ + 16 | 4-28 | Reserved                |
| BLKSZ - 32    | 4    | CRC32 Checksum          |

$$ N = \lfloor\frac{(Block\ Size)}{40}\rfloor $$

### DHB Entry

| Offset | Size | Value                |
|--------|------|----------------------|
| 0      | 8    | MDB Pointer          |
| 8      | 4    | Filename Hash        |
| 12     | 6    | Reserved             |
| 18     | 2    | Left Child Offset    |
| 20     | 2    | Right Child Offset   |
| 22     | 2    | Next Child Offset    |
| 24     | 8    | Left Child Block     |
| 32     | 8    | Right Child Block    |
| 40     | 8    | Next Child Block     |

## DDB - Data Descriptor Block

| Offset     | Size | Value                |
|------------|------|----------------------|
| 0          | 8    | Previous DDB Pointer |
| 8          | 8    | Next DDB Pointer     |
| 16         | 8    | 1st DSB Pointer      |
| 24         | 8    | 2nd DSB Pointer      |
| 32         | 8    | 3rd DSB Pointer      |
| ...        | 8    | $N$ th DSB Pointer   |
| BLKSZ - 16 | 12   | Reserved             |
| BLKSZ - 4  | 4    | CRC32 Checksum       |

$$ N = \frac{(Block\ Size)}{8} - 4 $$

## DSB - Data Storage Block

## JBB - Journal Buffer Block

## UDB - Usage Descriptor Block

| Offset    | Size      | Value          |
|-----------|-----------|----------------|
| 0         | 

### UDB Entry

| Offset    | Size      | Value            |
|-----------|-----------|------------------|
| 0         | 8         | Free Block Start |

## Formatting

total_block_count = (total_sector_count - reserved_sectors) / sectors_per_block
bytes_per_block = bytes_per_sector * sectors_per_block
blocks_per_block_group = bytes_per_block * 8 - 32
total_block_group_count = total_block_count / blocks_per_block_group
rbb_count = (total_block_group_count + blocks_per_block_group - 1) / blocks_per_block_group

ex)
sectors_per_block = 8, total_sector_count = 1073741824, reserved_sectors = 16, bytes_per_sector = 512

total_block_count = (1073741824 - 16) / 8 = 134217726
bytes_per_block = 512 * 8 = 4096
blocks_per_block_group = 4096 * 8 - 32 = 32736
total_block_group_count = 134217726 / 32736 = 4100
rbb_count = (4100 + 32736 - 1) / 32736 = 1


