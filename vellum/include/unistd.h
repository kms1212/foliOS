#ifndef __UNISTD_H__
#define __UNISTD_H__

#include <stdio.h>
#include <sys/types.h>

int creat(const char *path, mode_t mode);
int open(const char *path, int flags, ...);

off_t lseek(int fd, off_t offset, int whence);
ssize_t read(int fd, void *buf, size_t count);
ssize_t write(int fd, const void *buf, size_t count);
int close(int fd);

#endif // __UNISTD_H__
