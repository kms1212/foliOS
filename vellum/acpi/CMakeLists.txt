cmake_minimum_required(VERSION 3.13)

target_sources(vellum PRIVATE alloc.c event.c fwreq.c init.c isr.c log.c map.c misc.c mutex.c spinlock.c time.c)
