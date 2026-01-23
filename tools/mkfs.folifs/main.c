#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <sys/stat.h>
#include <getopt.h>

#include "folifs.h"

extern unsigned int crc32(const unsigned char *buf, unsigned int len);

static int decode_size(const char *str, uint64_t *value)
{
    uint64_t result = 0;

    for (int i = 0; str[i]; i++) {
        if (!isdigit(str[i])) {
            if (str[i + 1]) {
                return 1;
            }

            switch (str[i]) {
                case 'T':
                    result *= 1024;
                case 'G':
                    result *= 1024;
                case 'M':
                    result *= 1024;
                case 'k':
                    result *= 1024;
                    break;
                default:
                    return 1;
            }
            
            break;
        }

        result *= 10;
        result += str[i] - '0';
    }

    *value = result;
    return 0;
}

static int decode_uint(const char *str, uint64_t *value)
{
    uint64_t result = 0;

    if (str[0] != '0') {
        for (int i = 0; str[i]; i++) {
            if (!isdigit(str[i])) {
                return 1;
            }

            result *= 10;
            result += str[i] - '0';
        }

        *value = result;
        return 0;
    }

    if (str[1] == 'x') {
        for (int i = 2; str[i]; i++) {
            if (!isxdigit(str[i])) {
                return 1;
            }

            result <<= 4;
            result |= str[i] <= '9' ? str[i] - '0' : ((str[i] <= 'F' ? str[i] - 'A' : str[i] - 'a') + 10);
        }

        *value = result;
        return 0;
    }
    
    if (str[1] == 'o' || isdigit(str[1])) {
        for (int i = str[1] == 'o' ? 2 : 1; str[i]; i++) {
            if (!isdigit(str[i]) || str[i] >= '8') {
                return 1;
            }

            result <<= 3;
            result |= str[i] - '0';
        }

        *value = result;
        return 0;
    }
    
    if (str[1] == 'b') {
        for (int i = 2; str[i]; i++) {
            if (str[i] != '0' && str[i] != '1') {
                return 1;
            }

            result <<= 1;
            result |= str[i] - '0';
        }

        *value = result;
        return 0;
    }

    *value = result;
    return 0;
}

static int decode_uuid(const char *str, folifs_uuid_t *value)
{
    memset(value, 0, sizeof(folifs_uuid_t));

    int digit_idx = 0;
    for (int i = 0; str[i]; i++) {
        if (!isxdigit(str[i]) && str[i] != '-') {
            return 1;
        }

        if (str[i] == '-') {
            if (i != 8 && i != 13 && i != 18 && i != 23) {
                return 1;
            } else {
                continue;
            }
        }

        if (digit_idx >= 32) {
            return 1;
        }

        value->bytes[digit_idx >> 1] <<= 4;
        value->bytes[digit_idx >> 1] |= str[i] <= '9' ? str[i] - '0' : ((str[i] <= 'F' ? str[i] - 'A' : str[i] - 'a') + 10);
        digit_idx++;
    }

    return 0;
}

static const char *argv0;
static int verbose = 0, dry_run = 0, image = 0;

static long folifs_fread(void *ptr, size_t size, size_t count, FILE *stream)
{
    return fread(ptr, size, count, stream);
}

static long folifs_fwrite(const void *ptr, size_t size, size_t count, FILE *stream)
{
    if (dry_run) {
        return count;
    }

    return fwrite(ptr, size, count, stream);
}

static int make_fs(const char *file, const char *boot_code_file, const char *os_name, const char *label, uint64_t offset, uint8_t rdb_count, uint8_t sectors_per_block, uint16_t reserved_sectors, uint64_t journal_size, folifs_uuid_t uuid)
{
    uint16_t bytes_per_sector = 512;
    uint64_t total_sector_count;
    uint64_t total_block_count;
    uint32_t bytes_per_block;
    uint32_t blocks_per_block_group;
    uint64_t total_block_group_count;
    uint64_t rbb_count;
    off_t file_len;
    int label_len;
    
    FILE *image_fp = NULL;
    FILE *boot_code_fp = NULL;

    void *boot_code, *buf;

    image_fp = fopen(file, "rb+");
    if (!image_fp) {
        fprintf(stderr, "%s: cannot open file: %s\n", argv0, file);
        return 1;
    }
    
    fseek(image_fp, 0, SEEK_END);
    file_len = ftello(image_fp);
    if (file_len % bytes_per_sector) {
        fprintf(stderr, "%s: file size is not a multiple of sector size\n", argv0);
        fclose(image_fp);
        return 1;
    }
    total_sector_count = file_len / bytes_per_sector;

    total_block_count = (total_sector_count - reserved_sectors) / sectors_per_block;
    bytes_per_block = bytes_per_sector * sectors_per_block;
    blocks_per_block_group = bytes_per_block * 8 - 32;
    total_block_group_count = total_block_count / blocks_per_block_group;
    rbb_count = (total_block_group_count + blocks_per_block_group - 1) / blocks_per_block_group;

    if (!total_block_count || !bytes_per_block || !blocks_per_block_group || !total_block_group_count || !rbb_count) {
        fprintf(stderr, "%s: file size is too small for this configuration\n", argv0);
        fclose(image_fp);
        return 1;
    }

    printf("Creating FOLIFS Volume (%llu sectors, %llu blocks, %llu block groups)\n", total_sector_count, total_block_count, total_block_group_count);
    
    if (boot_code_file) {
        boot_code_fp = fopen(boot_code_file, "rb");
        if (!boot_code_fp) {
            fprintf(stderr, "%s: cannot open file: %s\n", argv0, boot_code_file);
            return 1;
        }

        fseek(boot_code_fp, 0, SEEK_END);
        file_len = ftello(boot_code_fp);
        if (file_len > reserved_sectors * bytes_per_sector) {
            fprintf(stderr, "%s: boot code is larger than reserved sectors area\n", argv0);
            fclose(boot_code_fp);
            fclose(image_fp);
            return 1;
        }
        
        boot_code = malloc(file_len);

        fseek(boot_code_fp, 0, SEEK_SET);
        folifs_fread(boot_code, file_len, 1, boot_code_fp);

        fseek(image_fp, 0, SEEK_SET);
        folifs_fwrite(boot_code, file_len, 1, image_fp);
        fflush(image_fp);

        free(boot_code);
        fclose(boot_code_fp);
    }
    
    buf = malloc(sizeof(struct folifs_first_sector));
    
    /* write sector 0 */
    fseek(image_fp, 0, SEEK_SET);
    folifs_fread(buf, sizeof(struct folifs_first_sector), 1, image_fp);
    ((struct folifs_first_sector *)buf)->volume_offset = offset;
    ((struct folifs_first_sector *)buf)->filesystem_signature[0] = 'A';
    ((struct folifs_first_sector *)buf)->filesystem_signature[1] = 'F';
    ((struct folifs_first_sector *)buf)->filesystem_signature[2] = 'S';
    ((struct folifs_first_sector *)buf)->filesystem_signature[3] = '\0';
    ((struct folifs_first_sector *)buf)->reserved_sectors = reserved_sectors;
    ((struct folifs_first_sector *)buf)->vbr_signature = 0xAA55;
    fseek(image_fp, 0, SEEK_SET);
    folifs_fwrite(buf, sizeof(struct folifs_first_sector), 1, image_fp);
    fflush(image_fp);

    buf = realloc(buf, bytes_per_block);
    
    /* write RDB */
    for (int i = 0; i < rdb_count; i++) {
        fseek(image_fp, bytes_per_sector * reserved_sectors + bytes_per_block * i, SEEK_SET);
        memset(buf, 0, bytes_per_block);
        ((struct folifs_rdb *)buf)->total_sector_count = total_sector_count;
        ((struct folifs_rdb *)buf)->total_block_count = total_block_count;
        ((struct folifs_rdb *)buf)->sectors_per_block = sectors_per_block;
        ((struct folifs_rdb *)buf)->rdb_copy_count = rdb_count;
        ((struct folifs_rdb *)buf)->bytes_per_sector = bytes_per_sector;
        ((struct folifs_rdb *)buf)->flags = 0;
        ((struct folifs_rdb *)buf)->root_mdb_pointer = rdb_count + rbb_count + 1;
        ((struct folifs_rdb *)buf)->udb_pointer = 0;
        ((struct folifs_rdb *)buf)->jbb_pointer = 0;
        ((struct folifs_rdb *)buf)->rbb_pointer = 1;
        ((struct folifs_rdb *)buf)->group0_gbb_pointer = rdb_count + rbb_count;
        ((struct folifs_rdb *)buf)->volume_uuid = uuid;
        strncpy(((struct folifs_rdb *)buf)->formatted_os, os_name, sizeof(((struct folifs_rdb *)buf)->formatted_os));
        ((struct folifs_rdb *)buf)->filesystem_version = 0x0001;
        *(uint32_t *)((uint8_t *)buf + bytes_per_block - 4) = crc32(buf, bytes_per_block - 4);
        folifs_fwrite(buf, bytes_per_block, 1, image_fp);
        fflush(image_fp);
    }

    /* write RBB */
    fseek(image_fp, bytes_per_sector * reserved_sectors + bytes_per_block * rdb_count, SEEK_SET);
    memset(buf, 0, bytes_per_block);
    for (uint64_t i = 1; i < total_block_group_count; i = (i & ~0x07) + 8) {
        uint64_t groups_left = total_block_group_count - i;
        ((uint8_t *)buf)[i >> 3] = (groups_left < 8 ? ((1 << groups_left) - 1) : 0xFF) & (0xFF << (i & 0x07));
    }
    *(uint32_t *)((uint8_t *)buf + bytes_per_block - 4) = crc32(buf, bytes_per_block - 4);
    folifs_fwrite(buf, bytes_per_block, 1, image_fp);
    fflush(image_fp);

    /* write Group 0 GBB */
    fseek(image_fp, bytes_per_sector * reserved_sectors + bytes_per_block * (rdb_count + rbb_count), SEEK_SET);
    memset(buf, 0, bytes_per_block);
    for (uint64_t i = rdb_count + 2 + rbb_count; i < blocks_per_block_group; i = (i & ~0x07) + 8) {
        uint64_t blocks_left = blocks_per_block_group - i;
        ((uint8_t *)buf)[i >> 3] = (blocks_left < 8 ? ((1 << blocks_left) - 1) : 0xFF) & (0xFF << (i & 0x07));
    }
    *(uint32_t *)((uint8_t *)buf + bytes_per_block - 4) = crc32(buf, bytes_per_block - 4);
    folifs_fwrite(buf, bytes_per_block, 1, image_fp);
    fflush(image_fp);

    /* write root MDB */
    fseek(image_fp, bytes_per_sector * reserved_sectors + bytes_per_block * (rdb_count + 1 + rbb_count), SEEK_SET);
    memset(buf, 0, bytes_per_block);
    ((struct folifs_mdb *)buf)->parent_mdb_pointer = 0;
    ((struct folifs_mdb *)buf)->next_mdb_pointer = 0;
    ((struct folifs_mdb *)buf)->flags = 1;  /* directory */
    ((struct folifs_mdb *)buf)->attribute_entry_presence_bitmap = (label ? (1 << (MDB_ENTRY_FAE - 1)) : 0) | (1 << (MDB_ENTRY_TAE - 1));
    ((struct folifs_mdb *)buf)->file_size = 0;
    ((struct folifs_mdb *)buf)->dhb_ddb_pointer = rdb_count + rbb_count + 2;

    int mdb_offset = sizeof(struct folifs_mdb);

    /* FAE */
    if (label) {
        label_len = strnlen(label, 255);

        ((struct folifs_mdb_fae *)((uint8_t *)buf + mdb_offset))->header.entry_length = sizeof(struct folifs_mdb_fae);
        ((struct folifs_mdb_fae *)((uint8_t *)buf + mdb_offset))->header.entry_type = MDB_ENTRY_FAE;
        ((struct folifs_mdb_fae *)((uint8_t *)buf + mdb_offset))->filename_length = label_len;
        ((struct folifs_mdb_fae *)((uint8_t *)buf + mdb_offset))->filename_hash = crc32((void *)label, label_len);
        strncpy(((struct folifs_mdb_fae *)((uint8_t *)buf + mdb_offset))->filename, label, label_len);

        mdb_offset += sizeof(struct folifs_mdb_fae);
    }

    /* TAE */
    ((struct folifs_mdb_tae *)((uint8_t *)buf + mdb_offset))->header.entry_length = sizeof(struct folifs_mdb_tae);
    ((struct folifs_mdb_tae *)((uint8_t *)buf + mdb_offset))->header.entry_type = MDB_ENTRY_TAE;
    ((struct folifs_mdb_tae *)((uint8_t *)buf + mdb_offset))->create_time = time(NULL);
    ((struct folifs_mdb_tae *)((uint8_t *)buf + mdb_offset))->write_time = time(NULL);
    ((struct folifs_mdb_tae *)((uint8_t *)buf + mdb_offset))->read_time = time(NULL);

    *(uint32_t *)((uint8_t *)buf + bytes_per_block - 4) = crc32(buf, bytes_per_block - 4);
    folifs_fwrite(buf, bytes_per_block, 1, image_fp);
    fflush(image_fp);

    /* write root dhb */
    fseek(image_fp, bytes_per_sector * reserved_sectors + bytes_per_block * (rdb_count + 2 + rbb_count), SEEK_SET);
    memset(buf, 0, bytes_per_block);
    *(uint32_t *)((uint8_t *)buf + bytes_per_block - 4) = crc32(buf, bytes_per_block - 4);
    folifs_fwrite(buf, bytes_per_block, 1, image_fp);
    fflush(image_fp);

    free(buf);
    fclose(image_fp);

    printf("Filesystem used %llu blocks out of %llu blocks (%.2f%%)\n", rdb_count + 2 + rbb_count, total_block_count, (float)5 / total_block_count);

    return 0;
}

static const struct option options[] = {
    { "boot-code", 1, NULL, 'b' },
    { "dry-run", 0, NULL, 'd' },
    { "image", 0, NULL, 'i'},
    { "journal-size", 1, NULL, 'j' },
    { "label", 1, NULL, 'l' },
    { "rdb-count", 1, NULL, 'N' },
    { "os-name", 1, NULL, 'o' },
    { "reserve", 1, NULL, 'r' },
    { "sectors-per-block", 1, NULL, 's' },
    { "uuid", 1, NULL, 'u'},
    { "verbose", 0, NULL, 'v' },
    { NULL, 0, NULL, 0 },
};

static void print_usage(void)
{
    fprintf(stderr, "usage: %s [-b boot-code] [-j journal-size] [-l label]\n", argv0);
    fprintf(stderr, "\t[-N rdb-count] [-O os-name] [-o offset]\n");
    fprintf(stderr, "\t[-r reserve] [-s sectors-per-block] [-u uuid]\n");
    fprintf(stderr, "\t[-div] device/file\n");
}

int main(int argc, char **argv)
{
    int next_option;

    uint64_t tmp;
    char *boot_code_file = NULL, *os_name = "UNKNOWN", *label = NULL, *file = NULL;
    uint8_t rdb_count = 1, sectors_per_block = 8;
    uint16_t reserved_sectors = 16;
    uint64_t journal_size = 1024 * 1024, offset = 0;
    folifs_uuid_t uuid;

    argv0 = argv[0];

    do {
        next_option = getopt_long(argc, argv, "b:dij:l:N:o:r:s:u:v", options, NULL);

        switch (next_option) {
            case 'b':
                boot_code_file = optarg;
                break;
            case 'd':
                dry_run = 1;
                break;
            case 'i':
                image = 1;
                break;
            case 'j':
                if (decode_size(optarg, &tmp)) {
                    fprintf(stderr, "%s: journal-size: invalid size\n", argv0);
                    return 1;
                }
                journal_size = tmp;
                break;
            case 'l':
                label = optarg;
                break;
            case 'N':
                if (decode_uint(optarg, &tmp) || tmp >= 256) {
                    fprintf(stderr, "%s: mdb-count: invalid numeric value\n", argv0);
                    return 1;
                }
                rdb_count = tmp;
                break;
            case 'O':
                os_name = optarg;
                break;
            case 'o':
                if (decode_uint(optarg, &tmp)) {
                    fprintf(stderr, "%s: offset: invalid value\n", argv0);
                    return 1;
                }
                offset = tmp;
                break;
            case 'r':
                if (decode_uint(optarg, &tmp) || tmp >= 65536) {
                    fprintf(stderr, "%s: reserve: invalid numeric value\n", argv0);
                    return 1;
                }
                reserved_sectors = tmp;
                break;
            case 's':
                if (decode_uint(optarg, &tmp) || tmp >= 256) {
                    fprintf(stderr, "%s: sectors-per-block: invalid numeric value\n", argv0);
                    return 1;
                }
                sectors_per_block = tmp;
                break;
            case 'u':
                if (decode_uuid(optarg, &uuid)) {
                    fprintf(stderr, "%s: uuid: invalid UUID value\n", argv0);
                    return 1;
                }
                break;
            case 'v':
                verbose = 1;
                break;
            case '?':
                print_usage();
                return 1;
            case -1:
                break;
            default:
                abort();
        }
    } while (next_option != -1);

    for (int i = optind; i < argc; i++) {
        file = argv[i];
        break;
    }

    return make_fs(file, boot_code_file, os_name, label, offset, rdb_count, sectors_per_block, reserved_sectors, journal_size, uuid);
}

