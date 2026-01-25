#ifndef __FCNTL_H__
#define __FCNTL_H__

#include <stdio.h>
#include <unistd.h>

#define O_CREAT     0x00000001
#define O_EXCL      0x00000002
#define O_NOCTTY    0x00000004
#define O_TRUNC     0x00000008
#define O_APPEND    0x00000010
#define O_DSYNC     0x00000020
#define O_NONBLOCK  0x00000040
#define O_RSYNC     0x00000080
#define O_SYNC      0x00000100
#define O_ACCMODE   0x00000600
#define O_RDONLY    0x00000200
#define O_RDWR      0x00000400
#define O_WRONLY    0x00000600

int fcntl(int fd, int op, ...);

#endif // __FCNTL_H__
