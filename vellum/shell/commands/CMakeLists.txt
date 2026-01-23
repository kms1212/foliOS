cmake_minimum_required(VERSION 3.13)

target_sources(vellum PRIVATE acpi.c chdir.c clear.c cpuid.c crc32.c dispimg.c echo.c in.c jump.c loadmodule.c lsdev.c lsfile.c lsfs.c lsmod.c lspci.c mem.c mount.c out.c poweroff.c read.c readblk.c readfile.c reboot.c testmouse.c testtty.c time.c usefont.c vmode.c write.c)
