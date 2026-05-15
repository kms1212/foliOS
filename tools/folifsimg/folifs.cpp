#include "folifs.hpp"

#include <cstring>

#include "crc32.h"
#include "folifs.h"

static void hexdump(const void *data, long len, uint32_t offset)
{
    const uint8_t *addr = (uint8_t *)data;
    long count = 0;
    uint8_t buf[16];

    while (count < len) {
        printf("%08lX │ ", count + offset);

        memcpy(buf, addr, sizeof(buf));

        for (int i = 0; i < sizeof(buf) && count + i < len; i++) {
            printf("%02X ", buf[i]);
        }

        printf("│ ");

        for (int i = 0; i < sizeof(buf) && count + i < len; i++) {
            printf("%c", buf[i] >= 0x20 && buf[i] < 0x80 ? (char)buf[i] : '.');
        }

        printf("\n");

        addr += 16;
        count += 16;
    }
}

Afs::Afs(Image &image, lba_t offset) : image(image), offset(offset)
{
    std::unique_ptr<uint8_t[]> read_buf = std::make_unique<uint8_t[]>(512);

    image.read(read_buf.get(), offset, 1);
    const auto *first_sector = (const struct folifs_first_sector *)read_buf.get();

    if (strncmp(first_sector->filesystem_signature, "FOLIFS", 4) != 0) {
        fprintf(stderr, "invalid signature\n");
        exit(1);
    }

    if (first_sector->vbr_signature != 0xAA55) {
        fprintf(stderr, "invalid signature\n");
        exit(1);
    }

    this->reserved_sectors = first_sector->reserved_sectors;

    image.read(read_buf.get(), offset + this->reserved_sectors, 1);
    const auto *rdb = (const struct folifs_rdb *)read_buf.get();

    this->bytes_per_sector = rdb->bytes_per_sector;
    this->sectors_per_block = rdb->sectors_per_block;
    this->bytes_per_block = this->bytes_per_sector * this->sectors_per_block;

    read_buf.release();
    read_buf = std::make_unique<uint8_t[]>(this->bytes_per_block);

    this->readBlock(read_buf.get(), 0, 1, true);
    rdb = (const struct folifs_rdb *)read_buf.get();

    this->total_sector_count = rdb->total_sector_count;
    this->total_block_count = rdb->total_block_count;
    this->rdb_copy_count = rdb->rdb_copy_count;
    this->flags = rdb->flags;
    this->root_mdb_pointer = rdb->root_mdb_pointer;
    this->udb_pointer = rdb->udb_pointer;
    this->jbb_pointer = rdb->jbb_pointer;
    this->rbb_pointer = rdb->rbb_pointer;
    this->group0_gbb_pointer = rdb->group0_gbb_pointer;
    memcpy(this->volume_uuid.bytes, rdb->volume_uuid.bytes, sizeof(this->volume_uuid.bytes));
    memcpy(this->formatted_os, rdb->formatted_os, sizeof(this->formatted_os));
}

long Afs::readBlock(void *buf, block_t blk, long count, bool check_crc)
{
    long read_count = 0;

    std::unique_ptr<uint8_t[]> read_buf = std::make_unique<uint8_t[]>(this->bytes_per_block);

    for (int i = 0; i < count; i++) {
        if (image.read(
                read_buf.get(),
                this->offset + this->reserved_sectors + blk * this->sectors_per_block,
                this->sectors_per_block
            ) != this->sectors_per_block) {
            break;
        }

        if (check_crc &&
            crc32(read_buf.get(), this->bytes_per_block - sizeof(uint32_t)) !=
                *(uint32_t *)(read_buf.get() + this->bytes_per_block - sizeof(uint32_t))) {
            fprintf(
                stderr,
                "crc32 error: lba %08llX, block %08llX\n",
                this->offset + this->reserved_sectors + blk * this->sectors_per_block,
                blk
            );
        }

        memcpy((uint8_t *)buf + i * this->bytes_per_block, read_buf.get(), this->bytes_per_block);

        read_count++;
    }

    return read_count;
}

Afs::Directory *Afs::openRootDirectory(void)
{
    return new Afs::Directory(*this, this->root_mdb_pointer);
}

Afs::Directory::Directory(Afs &folifs, block_t mdb) : folifs(folifs), mdb(mdb) {}

std::string Afs::Directory::getName(void)
{
    std::unique_ptr<uint8_t[]> read_buf =
        std::make_unique<uint8_t[]>(this->folifs.getBytesPerBlock());

    this->folifs.readBlock(read_buf.get(), this->mdb, 1, true);
    const auto *mdb = (const struct folifs_mdb *)read_buf.get();

    hexdump(read_buf.get(), this->folifs.getBytesPerBlock(), 0);

    if (!(mdb->attribute_entry_presence_bitmap & (1 << (MDB_ENTRY_FAE - 1)))) {
        return "";
    }

    const auto *mdb_fae = (const struct folifs_mdb_fae *)(read_buf.get() + sizeof(*mdb));

    return std::string(mdb_fae->filename);
}
