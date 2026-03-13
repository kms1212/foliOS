#include <vellum/path.h>

#include <string.h>

void VlPath_InitIter(struct path_iterator *it, const char *path)
{
    it->path = path;
    it->cursor = path;
    it->has_separator = 0;

    const char *colon = strchr(path, ':');
    if (colon) {
        size_t len = colon - path;
        if (len >= FILENAME_MAX) len = FILENAME_MAX - 1;
        memcpy(it->element, path, len);
        it->element[len] = '\0';
        it->cursor = colon + 1;
    } else {
        it->element[0] = '\0';
    }
}

int VlPath_AdvanceIter(struct path_iterator *it)
{
    const char *p = it->cursor;
    it->has_separator = (*p == '/');
    if (it->has_separator) {
        p++;
    }

    if (*p == '\0') {
        it->element[0] = '\0';
        it->cursor = p;
        return 1;
    }

    const char *slash = strchr(p, '/');
    size_t len;
    if (slash) {
        len = slash - p;
        it->cursor = slash;
    } else {
        len = strlen(p);
        it->cursor = p + len;
    }

    if (len >= FILENAME_MAX) len = FILENAME_MAX - 1;
    memcpy(it->element, p, len);
    it->element[len] = '\0';

    return *it->cursor == '\0';
}

char *VlPath_Join(char *dest, size_t len, const char *src)
{
    size_t dest_slen = strnlen(dest, len);
    char *cur = dest + dest_slen;

    if (len > dest_slen) {
        *cur++ = '/';
        len--;
    }

    while (len > dest_slen && *src) {
        *cur++ = *src++;
        len--;
    }

    if (len > dest_slen) {
        *cur = '\0';
    }

    return dest;
}

char *VlPath_Normalize(char *dest, size_t len, const char *src)
{
    if (!dest || len == 0 || !src) {
        return NULL;
    }

    struct path_iterator it;
    VlPath_InitIter(&it, src);

    char *write_pos = dest;
    char *dest_end = dest + len - 1;
    int is_absolute = 0;
    int has_filesystem = 0;
    int first_loop = 1;

    if (it.element[0] != '\0') {
        has_filesystem = 1;
        size_t prefix_len = strlen(it.element);
        if (write_pos + prefix_len + 1 >= dest_end) {
            dest[0] = '\0';
            return dest;
        }
        strcpy(write_pos, it.element);
        write_pos += prefix_len;
        *write_pos++ = ':';
    }

    int asdf;
    do {
        asdf = VlPath_AdvanceIter(&it);

        if (first_loop) {
            if (it.has_separator) {
                is_absolute = 1;
                if (write_pos < dest_end) {
                    *write_pos++ = '/';
                }
            }
            first_loop = 0;
        }

        if (it.element[0] == '\0') continue;

        if (strcmp(it.element, ".") == 0) continue;

        if (strcmp(it.element, "..") == 0) {
            if (is_absolute) {
                if (write_pos > dest) {
                    char *last_sep = write_pos - 1;

                    if (*last_sep == '/') {
                        last_sep--;
                    }

                    while (last_sep > dest && *last_sep != '/') {
                        last_sep--;
                    }

                    if (has_filesystem) {
                        char *colon_pos = strchr(dest, ':');
                        if (colon_pos && last_sep <= colon_pos + 1) {
                            write_pos = colon_pos + 1;
                            if (write_pos < dest_end) {
                                *write_pos++ = '/';
                            }
                            continue;
                        }
                    }

                    if (last_sep <= dest) {
                        write_pos = dest + (has_filesystem ? strchr(dest, ':') - dest + 1 : 0);
                        if (write_pos < dest_end) {
                            *write_pos++ = '/';
                        }
                        continue;
                    }

                    write_pos = last_sep + 1;
                }
            } else if (has_filesystem) {
                char *colon_pos = strchr(dest, ':');
                if (write_pos > colon_pos + 1) {
                    char *last_sep = write_pos - 1;

                    if (*last_sep == '/') {
                        last_sep--;
                    }

                    while (last_sep > colon_pos + 1 && *last_sep != '/') {
                        last_sep--;
                    }

                    if (last_sep <= colon_pos + 1) {
                        write_pos = colon_pos + 1;
                    } else {
                        write_pos = last_sep + 1;
                    }
                } else {
                    if (write_pos + 2 < dest_end) {
                        *write_pos++ = '.';
                        *write_pos++ = '.';
                    }
                }
            } else {
                if (write_pos > dest && write_pos < dest_end) {
                    *write_pos++ = '/';
                }

                if (write_pos + 2 < dest_end) {
                    *write_pos++ = '.';
                    *write_pos++ = '.';
                }
            }
            continue;
        }

        if (write_pos > dest && write_pos[-1] != '/' && write_pos < dest_end) {
            *write_pos++ = '/';
        }

        size_t elem_len = strlen(it.element);
        if (write_pos + elem_len < dest_end) {
            strcpy(write_pos, it.element);
            write_pos += elem_len;
        }

    } while (!asdf);

    if (!has_filesystem && write_pos == dest && dest_end - dest > 0) {
        *dest = '.';
        write_pos = dest + 1;
    }

    if (write_pos > dest + 1 && *(write_pos - 1) == '/') {
        if (!has_filesystem || write_pos > strchr(dest, ':') + 2) {
            write_pos--;
        }
    }

    if (write_pos < dest + len) {
        *write_pos = '\0';
    } else {
        dest[len - 1] = '\0';
    }

    return dest;
}

char *VlPath_GetFsname(char *buf, size_t len, const char *path)
{
    const char *fsname_end = strchr(path, ':');
    if (!fsname_end) {
        buf[0] = '\0';
        return buf;
    }

    for (size_t i = 0; i < len && path < fsname_end; i++) {
        buf[i] = path[i];
    }

    return buf;
}

char *VlPath_GetDirname(char *buf, size_t len, const char *path)
{
    const char *dirname_end = strrchr(path, '/');
    if (!dirname_end) {
        dirname_end = path + strlen(path);
    }

    for (size_t i = 0; i < len && path < dirname_end; i++) {
        buf[i] = path[i];
    }

    return buf;
}

char *VlPath_GetBasename(char *buf, size_t len, const char *path)
{
    return buf;
}

char *VlPath_GetStem(char *buf, size_t len, const char *path)
{
    return buf;
}

char *VlPath_GetExtension(char *buf, size_t len, const char *path)
{
    return buf;
}

int VlPath_IsAbsolute(const char *path)
{
    if (path[0] == '/') {
        return 1;
    }

    const char *fsname_end = strchr(path, ':');
    if (fsname_end && (fsname_end[1] == '/' || fsname_end[1] == '\0')) {
        return 1;
    }

    return 0;
}

int VlPath_Compare(const char *path1, const char *path2, int case_sensitive);
