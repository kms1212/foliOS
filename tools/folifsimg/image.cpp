#include "image.hpp"

#include <cstdlib>
#include <cstring>
#include <cerrno>

Image::Image(const std::string &path, bool readonly) : readonly(readonly)
{
    this->fp = fopen(path.c_str(), readonly ? "rb" : "rb+");
    if (!this->fp) {
        fprintf(stderr, "%s", strerror(errno));
        exit(1);
    }
}

Image::~Image()
{
    fclose(this->fp);
}

long Image::read(void *buf, lba_t lba, long count)
{
    if (count < 1) return 0;

    fseek(this->fp, lba * 512, SEEK_SET);
    count = fread(buf, 512, count, this->fp);

    return count;
}

long Image::write(const void *buf, lba_t lba, long count)
{
    if (count < 1) return 0;

    fseek(this->fp, lba * 512, SEEK_SET);
    count = fwrite(buf, 512, count, this->fp);
    fflush(this->fp);

    return count;
}
