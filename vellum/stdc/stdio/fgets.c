#include <stdio.h>

#include <string.h>

char *fgets(char *__restrict str, int num, FILE *__restrict stream)
{
    int cur = 0;
    char ch;
    do {
        fread(&ch, 1, 1, stream);
        switch (ch) {
            case '\r':
            case '\n':
                if (cur >= num - 1) break;
                putchar('\n');
                str[cur++] = '\n';
                break;
            case '\b':
                if (cur < 1) {
                    break;
                }
                cur--;
                fputs("\b \b", stdout);
                break;
            default:
                if (cur >= num - 1) break;
                putchar(ch);
                str[cur++] = ch;
                break;
        }
    } while (ch != '\r' && ch != '\n');

    str[cur] = 0;

    return str;
}
