cmake_minimum_required(VERSION 3.13)

target_sources(stage1 PRIVATE bioscall.S bootnext.S disk.c keyboard.c mem.c misc.c video.c)
