#include <vellum/shell.h>

#include <stdio.h>

long VlShell_Readline(const char *__restrict prompt, char *__restrict buf, long len)
{
    fputs(prompt, stdout);

    int cur = 0;
    char ch;
    do {
        fread(&ch, 1, 1, stdin);
        switch (ch) {
        case '\r':
        case '\n':
            putchar('\n');
            break;
        case '\b':
            if (cur < 1) {
                break;
            }
            cur--;
            fputs("\b \b", stdout);
            break;
        default:
            if (cur >= len - 1) break;
            putchar(ch);
            buf[cur++] = ch;
            break;
        }
    } while (ch != '\r' && ch != '\n');

    buf[cur] = 0;
    return cur;
}
