#include <vellum/shell.h>

#include <stdio.h>
#include <string.h>

#include <vellum/filesystem.h>
#include <vellum/path.h>

static int list_directory(struct fs_directory *dir)
{
    status_t status;
    struct fs_directory_entry direntry;

    status = dir->fs->driver->rewind_directory(dir);
    if (!CHECK_SUCCESS(status)) return 1;

    for (;;) {
        status = dir->fs->driver->iter_directory(dir, &direntry);
        if (!CHECK_SUCCESS(status)) {
            if (status == STATUS_END_OF_LIST) break;
            return 1;
        }

        if (strcmp(direntry.name, ".") == 0 || strcmp(direntry.name, "..") == 0) {
            continue;
        }

        printf("%8llu  %s\n", direntry.size, direntry.name);
    }

    return 0;
}

static int lsfile_handler(struct shell_instance *inst, int argc, char **argv)
{
    status_t status;

    if (argc < 2) {
        if (!inst->working_dir) {
            fprintf(stderr, "%s: filesystem not selected\n", argv[0]);
            return 1;
        }

        return list_directory(inst->working_dir);
    }

    if (VlPath_IsAbsolute(argv[1])) {
        struct path_iterator iter;
        VlPath_InitIter(&iter, argv[1]);

        struct filesystem *newfs;
        if (iter.element[0]) {
            status = VlFs_Find(iter.element, &newfs);
            if (!CHECK_SUCCESS(status)) {
                fprintf(stderr, "%s: failed to open filesystem\n", argv[0]);
                return 1;
            }
        } else {
            newfs = inst->fs;
            if (!newfs) {
                fprintf(stderr, "%s: no open filesystem\n", argv[0]);
                return 1;
            }
        }

        struct fs_directory *newdir;
        status = newfs->driver->open_root_directory(newfs, &newdir);
        if (!CHECK_SUCCESS(status)) {
            fprintf(stderr, "%s: failed to open root directory\n", argv[0]);
            return 1;
        }

        int iter_result;
        do {
            iter_result = VlPath_AdvanceIter(&iter);
            if (iter.element[0] == '\0') {
                continue;
            }
            if (strcmp(".", iter.element) == 0) {
                continue;
            }

            struct fs_directory *tmp;
            status = newfs->driver->open_directory(newdir, iter.element, &tmp);
            newfs->driver->close_directory(newdir);

            if (!CHECK_SUCCESS(status)) {
                fprintf(stderr, "%s: failed to open directory\n", argv[0]);
                return 1;
            }

            newdir = tmp;
        } while (!iter_result);

        return list_directory(newdir);
    } else {
        if (!inst->fs) {
            fprintf(stderr, "%s: filesystem not selected\n", argv[0]);
            return 1;
        }

        struct path_iterator iter;
        VlPath_InitIter(&iter, argv[1]);

        struct fs_directory *newdir = inst->working_dir;
        int iter_result;
        do {
            iter_result = VlPath_AdvanceIter(&iter);
            if (iter.element[0] == '\0') {
                continue;
            }
            if (strcmp(".", iter.element) == 0) {
                continue;
            }

            struct fs_directory *tmp;
            status = inst->fs->driver->open_directory(newdir, iter.element, &tmp);
            if (newdir != inst->working_dir) {
                inst->fs->driver->close_directory(newdir);
            }

            if (!CHECK_SUCCESS(status)) {
                fprintf(stderr, "%s: failed to open directory\n", argv[0]);
                return 1;
            }

            newdir = tmp;
        } while (!iter_result);

        return list_directory(newdir);
    }
}

static struct command lsfile_command = {
    .name = "lsfile",
    .handler = lsfile_handler,
    .help_message = "List files of a directory",
};

static void lsfile_command_init(void)
{
    VlShell_RegisterCommand(&lsfile_command);
}

REGISTER_SHELL_COMMAND(lsfile, lsfile_command_init)
