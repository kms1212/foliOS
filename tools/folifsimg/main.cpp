#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "folifs.hpp"
#include "image.hpp"

int main(int argc, char **argv)
{
    if (argc < 1) {
        fprintf(stderr, "usage: %s image[@@offset] subcommand [...]\n", argv[0]);
        return 1;
    }

    lba_t offset = 0;
    char *sep_pos = strstr(argv[1], "@@");
    if (sep_pos) {
        *sep_pos = '\0';
        offset = strtoll(sep_pos + 2, NULL, 10);
    }

    std::string image_offset(argv[1]);

    Image image(argv[1], true);

    Afs folifs(image, offset);

    printf("total sector count: %llu\n", folifs.getTotalSectorCount());
    printf("total block count: %llu\n", folifs.getTotalBlockCount());
    printf("rdb copy count: %u\n", folifs.getRdbCopyCount());
    printf("bytes per sector: %u\n", folifs.getBytesPerSector());
    printf("sectors per block: %u\n", folifs.getSectorsPerBlock());

    std::unique_ptr<Afs::Directory> root_dir =
        std::unique_ptr<Afs::Directory>(folifs.openRootDirectory());

    printf("volume name: %s\n", root_dir->getName().c_str());

    return 0;
}
