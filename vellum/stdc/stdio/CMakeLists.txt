cmake_minimum_required(VERSION 3.13)

target_sources(vellum PRIVATE __fprintf_chk.c __printf_chk.c __snprintf_chk.c __sprintf_chk.c __vfprintf_chk.c __vprintf_chk.c __vsnprintf_chk.c __vsprintf_chk.c)
target_sources(vellum PRIVATE cprintf.c fclose.c fflush.c fgetc.c fgets.c fopen.c fprintf.c fputc.c fputs.c fseek.c ftell.c freopencookie.c freopendevice.c fread.c fwrite.c printf.c putc.c putchar.c puts.c snprintf.c sscanf.c sprintf.c stdio.c vcprintf.c vprintf.c vsnprintf.c vsprintf.c vfprintf.c)
