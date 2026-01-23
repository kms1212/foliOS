#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <sys/stat.h>

#include "folifs.h"
#include "command.h"

const char *argv0;

struct subcommand {
    struct subcommand *next;
    const char *name;
    int (*handler)(int argc, char **argv);
    const char *usage_str;
};

static const struct subcommand subcommand_list[] = {
    {
        .name = "volinfo",
        .handler = volinfo_handler,
        .usage_str = "[-i image]\n",
    },
    {
        .name = "mkdir",
        .handler = mkdir_handler,
        .usage_str = "[-i image]\n",
    },
    {
        .name = "copy",
        .handler = copy_handler,
        .usage_str = "[-i image]\n",
    },
    {
        .name = "move",
        .handler = move_handler,
        .usage_str = "[-i image]\n",
    },
    {
        .name = "delete",
        .handler = delete_handler,
        .usage_str = "[-i image]\n",
    },
    {
        .name = "lsfile",
        .handler = lsfile_handler,
        .usage_str = "[-i image]\n",
    },
    {
        .name = "setattr",
        .handler = setattr_handler,
        .usage_str = "[-i image]\n",
    },
    {
        .name = "getattr",
        .handler = getattr_handler,
        .usage_str = "[-i image]\n",
    },
};

static const struct subcommand *find_subcommand(const char *subcmd_name)
{
    for (int i = 0; i < sizeof(subcommand_list) / sizeof(*subcommand_list); i++) {
        if (strcmp(subcommand_list[i].name, subcmd_name) == 0) {
            return &subcommand_list[i];
        }
    }

    return NULL;
}

static void print_usage(const struct subcommand *subcmd)
{
    if (!subcmd) {
        fprintf(stderr, "usage: %s subcommand [-h] [args...]\navailable subcommands: ", argv0);
        for (int i = 0; i < sizeof(subcommand_list) / sizeof(*subcommand_list); i++) {
            fprintf(stderr, "%s ", subcommand_list[i].name);
        }
        fprintf(stderr, "\n");
    } else {
        fprintf(stderr, "usage: %s %s %s", argv0, subcmd->name, subcmd->usage_str);
    }
}

int main(int argc, char **argv)
{
    int next_option;
    int help = 0;

    argv0 = argv[0];

    const char *subcmd_name = NULL;
    if (argc > 1) {
        subcmd_name = argv[1];
    } else {
        print_usage(NULL);
        return 1;
    }

    const struct subcommand *subcmd = find_subcommand(subcmd_name);
    if (help) {
        print_usage(subcmd);
        return 0;
    }

    if (!subcmd) {
        print_usage(NULL);
        return 1;
    }

    int ret = subcmd->handler(argc, argv);
    if (ret) {
        print_usage(subcmd);
        return 1;
    }
    return 0;
}

