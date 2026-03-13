#include <vellum/shell.h>

#include <stdio.h>
#include <string.h>

#include <vellum/filesystem.h>
#include <vellum/path.h>
#include <vellum/status.h>

static int chdir_handler(struct shell_instance *inst, int argc, char **argv)
{
    status_t status;

    if (argc < 2) {
        fprintf(stderr, "usage: %s dir_name\n", argv[0]);
        return 1;
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

        if (inst->working_dir) {
            inst->working_dir->fs->driver->close_directory(inst->working_dir);
        }
        inst->working_dir = newdir;
        inst->fs = newfs;

        char pathbuf[PATH_MAX];
        if (!VlPath_GetFsname(pathbuf, sizeof(pathbuf), argv[1])[0]) {
            snprintf(pathbuf, sizeof(pathbuf), "%s:%s", inst->fs->name, argv[1]);
        } else {
            strncpy(pathbuf, argv[1], sizeof(pathbuf) - 1);
        }
        VlPath_Normalize(inst->working_dir_path, sizeof(inst->working_dir_path), pathbuf);
    } else {
        if (!inst->fs) {
            fprintf(stderr, "%s: filesystem not selected\n", argv[0]);
            return 1;
        }

        char pathbuf[PATH_MAX];
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

        strncpy(pathbuf, inst->working_dir_path, sizeof(pathbuf));
        VlPath_Join(pathbuf, sizeof(pathbuf), argv[1]);

        if (inst->working_dir) {
            inst->working_dir->fs->driver->close_directory(inst->working_dir);
        }
        inst->working_dir = newdir;

        VlPath_Normalize(inst->working_dir_path, sizeof(inst->working_dir_path), pathbuf);
    }

    return 0;
}

static struct command cd_command = {
    .name = "cd",
    .handler = chdir_handler,
    .help_message = "Change working directory",
};

static struct command chdir_command = {
    .name = "chdir",
    .handler = chdir_handler,
    .help_message = "Change working directory",
};

static void chdir_command_init(void)
{
    VlShell_RegisterCommand(&cd_command);
    VlShell_RegisterCommand(&chdir_command);
}

REGISTER_SHELL_COMMAND(chdir, chdir_command_init)
