#ifndef __FOLIFS_HPP__
#define __FOLIFS_HPP__

#include "types.h"
#include "image.hpp"

typedef int64_t block_t;

class Afs {
private:
    Image &image;
    const lba_t offset;

    uint16_t reserved_sectors;
    uint64_t total_sector_count;
    uint64_t total_block_count;
    uint8_t sectors_per_block;
    uint8_t rdb_copy_count;
    uint16_t bytes_per_sector;
    uint32_t flags;
    uint64_t root_mdb_pointer;
    uint64_t udb_pointer;
    uint64_t jbb_pointer;
    uint64_t rbb_pointer;
    uint64_t group0_gbb_pointer;
    uuid_t volume_uuid;
    char formatted_os[16];
    uint16_t filesystem_version;
    uint16_t bytes_per_block;

    long readBlock(void *buf, block_t blk, long count, bool check_crc);
    long writeBlock(const void *buf, block_t blk, long count, bool insert_crc);

public:
    class File {
    private:
        Afs &folifs;
        const block_t mdb;

    public:
        File(Afs &folifs, block_t mdb);
        ~File();

        long read(void *buf, size_t size, size_t count);
        long write(const void *buf, size_t size, size_t count);
    };

    class Directory {
    private:
        Afs &folifs;
        const block_t mdb;
    
    public:
        Directory(Afs &folifs, block_t mdb);
    
        std::string getName(void);
        
        Afs::File *openFile(const std::string &name);
        Afs::Directory *openDirectory(const std::string &name);
    };
    
    Afs(Image &image, lba_t offset);

    Afs::Directory *openRootDirectory(void);

    lba_t getOffset(void) const { return this->offset; }
    uint16_t getReservedSectors(void) const { return this->reserved_sectors; }
    uint64_t getTotalSectorCount(void) const { return this->total_sector_count; }
    uint64_t getTotalBlockCount(void) const { return this->total_block_count; }
    uint8_t getRdbCopyCount(void) const { return this->rdb_copy_count; }
    uint16_t getBytesPerSector(void) const { return this->bytes_per_sector; }
    uint8_t getSectorsPerBlock(void) const { return this->sectors_per_block; }
    uint16_t getBytesPerBlock(void) const { return this->bytes_per_block; }
    uuid_t getVolumeUuid(void) const { return this->volume_uuid; }
    std::string getFormattedOs(void) const { return std::string(this->formatted_os); }
    uint16_t getFilesystemVersion(void) const { return this->filesystem_version; }
};

#endif // __FOLIFS_HPP__
