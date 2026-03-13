#include <vellum/shell.h>

#include <ctype.h>
#include <stdlib.h>

#define PUSH_CHAR_E(buf, buflen, ch)                                                               \
    do {                                                                                           \
        if (buflen > 0) {                                                                          \
            *buf++ = ch;                                                                           \
            buflen--;                                                                              \
        } else {                                                                                   \
            return STATUS_BUFFER_TOO_SMALL;                                                        \
        }                                                                                          \
    } while (0)

#define PUSH_CHAR(buf, buflen, ch)                                                                 \
    do {                                                                                           \
        if (buflen > 0) {                                                                          \
            *buf++ = ch;                                                                           \
            buflen--;                                                                              \
        } else {                                                                                   \
            return NULL;                                                                           \
        }                                                                                          \
    } while (0)

status_t VlShell_Expand(struct shell_instance *inst, const char *line, char *buf, long buflen)
{
    status_t status;
    char var_key[64];
    int var_key_len;
    const char *var_value;

    while (*line) {
        if (*line == '\\' && line[1] == '$') {
            PUSH_CHAR_E(buf, buflen, '$');
            line += 2;
        } else if (*line == '$') {
            line++;
            var_key_len = 0;
            while ((isalnum(*line) || *line == '_') && var_key_len < sizeof(var_key) - 1) {
                var_key[var_key_len++] = *line++;
            }
            var_key[var_key_len] = '\0';

            status = VlShell_GetVariable(inst, var_key, &var_value);
            if (!CHECK_SUCCESS(status)) {
                var_value = "";
            }

            for (int i = 0; var_value[i]; i++) {
                PUSH_CHAR_E(buf, buflen, var_value[i]);
            }
        } else {
            PUSH_CHAR_E(buf, buflen, *line);
            line++;
        }
    }

    if (buflen > 0) {
        *buf = 0;
    }

    return STATUS_SUCCESS;
}

const char *VlShell_Parse(const char *line, char *buf, long buflen)
{
    int escape = 0;
    char quote_char = 0;

    while (*line == ' ' || *line == '\t') {
        line++;
    }

    if (!*line) return NULL;

    while (*line) {
        switch (*line) {
        case '"':
        case '\'':
            quote_char = quote_char == *line ? 0 : *line;
            line++;
            break;
        case '\\':
            escape = 1;
            break;
        case ' ':
        case '\t':
            if (quote_char) {
                PUSH_CHAR(buf, buflen, *line++);
            } else {
                goto end;
            }
            break;
        default:
            PUSH_CHAR(buf, buflen, *line++);
            break;
        }

        if (escape) {
            switch (*++line) {
            case '\\':
            case '\'':
            case '\"':
            case '?':
            case ' ':
            case '\t':
                PUSH_CHAR(buf, buflen, *line++);
                break;
            case 'a':
                PUSH_CHAR(buf, buflen, '\a');
                break;
            case 'b':
                PUSH_CHAR(buf, buflen, '\b');
                break;
            case 'e':
                PUSH_CHAR(buf, buflen, '\033');
                break;
            case 'f':
                PUSH_CHAR(buf, buflen, '\f');
                break;
            case 'n':
                PUSH_CHAR(buf, buflen, '\n');
                break;
            case 'r':
                PUSH_CHAR(buf, buflen, '\r');
                break;
            case 't':
                PUSH_CHAR(buf, buflen, '\t');
                break;
            case 'v':
                PUSH_CHAR(buf, buflen, '\v');
                break;
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7': {
                char *newptr = (char *)line;
                PUSH_CHAR(buf, buflen, strtol(line, &newptr, 8));
                line = newptr;
                break;
            }
            case 'x': {
                line++;
                char *newptr = (char *)line;
                PUSH_CHAR(buf, buflen, strtol(line, &newptr, 16));
                line = newptr;
                break;
            }
            }

            escape = 0;
        }
    }

end:
    if (buflen > 0) {
        *buf = 0;
    }

    return line;
}
