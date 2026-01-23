#ifndef __IMAGE_HPP__
#define __IMAGE_HPP__

#include <cstdio>

#include <string>

#include "types.h"

class Image {
private:
    FILE *fp;
    const bool readonly;

public:
    Image(const std::string &path, bool readonly);
    ~Image();

    long read(void *buf, lba_t lba, long count);
    long write(const void *buf, lba_t lba, long count);
};

#endif // __IMAGE_HPP__
