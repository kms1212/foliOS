#include <vellum/shell.h>

#include <ctype.h>
#include <getopt.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vellum/filesystem.h>
#include <vellum/macros.h>

static struct command *command_list_head = NULL;

static int set_handler(struct shell_instance *inst, int argc, char **argv)
{
    status_t status;

    if (argc != 3) {
        fprintf(stderr, "usage: %s key value\n", argv[0]);
        return 1;
    }

    status = VlShell_SetVariable(inst, argv[1], argv[2]);
    if (!CHECK_SUCCESS(status)) return 1;

    return 0;
}

static int help_handler(struct shell_instance *inst, int argc, char **argv)
{
    if (argc > 1) {
        int found = 0;
        for (struct command *cmd = command_list_head; cmd; cmd = cmd->next) {
            if (strcmp(cmd->name, argv[1]) == 0) {
                printf("%s\n", cmd->help_message);
                found = 1;
            }
        }

        if (!found) {
            puts("Command not found\n");
        }
    } else {
        for (struct command *cmd = command_list_head; cmd; cmd = cmd->next) {
            printf("\x1b[1m\x1b[94m%s\x1b[0m: %s\n", cmd->name, cmd->help_message);
        }
    }
    return 0;
}

static struct shell_instance global_inst = {
    .fs = NULL,
    .working_dir = NULL,
    .working_dir_path = {
        0,
    },
};

int VlShell_Execute(struct shell_instance *inst, const char *line)
{
    status_t status;
    char line_buf[4096];
    char elem_buf[512], *newargv[32];

    if (!inst) {
        inst = &global_inst;
    }

    char *elem_cursor = elem_buf;
    size_t elem_buf_len = sizeof(elem_buf);
    int newargc = 0;

    status = VlShell_Expand(inst, line, line_buf, sizeof(line_buf));
    if (!CHECK_SUCCESS(status)) {
        return 1;
    }
    line = line_buf;

    while (newargc < ARRAY_SIZE(newargv)) {
        line = VlShell_Parse(line, elem_cursor, elem_buf_len);
        newargv[newargc] = elem_cursor;
        if (!line) break;

        size_t elem_len = strnlen(elem_cursor, elem_buf_len);
        elem_cursor += elem_len + 1;
        elem_buf_len -= elem_len + 1;

        newargc++;
    }

    if (newargc < 1) {
        return 0;
    }

    if (strcmp("exit", newargv[0]) == 0) {
        return -1;
    }

    for (struct command *cmd = command_list_head; cmd; cmd = cmd->next) {
        if (strcmp(cmd->name, newargv[0]) == 0) {
            return cmd->handler(inst, newargc, newargv);
        }
    }

    printf("command not found: %s\n", newargv[0]);
    return 1;
}

int shell_handler(struct shell_instance *inst, int argc, char **argv)
{
    struct shell_instance new_inst = {
        .fs = NULL,
        .working_dir = NULL,
        .working_dir_path = {
            0,
        },
    };

    char line_buf[512];
    int result = 0;

    for (;;) {
        if (result) {
            printf("(%d) ", result);
        }
        if (new_inst.fs) {
            printf("%s", new_inst.working_dir_path);
        }

        VlShell_Readline("> ", line_buf, sizeof(line_buf));

        result = VlShell_Execute(&new_inst, line_buf);
        if (result < 0) break;
    }

    return 0;
}

status_t VlShell_RegisterCommand(struct command *cmd)
{
    if (!command_list_head) {
        command_list_head = cmd;
    } else {
        struct command *last_cmd = command_list_head;
        for (; last_cmd->next; last_cmd = last_cmd->next) {
        }

        last_cmd->next = cmd;
    }

    return STATUS_SUCCESS;
}

void VlShell_UnregisterCommand(struct command *cmd)
{
    if (!command_list_head) return;

    struct command *prev_cmd = NULL;
    for (struct command *current = command_list_head; current->next; current = current->next) {
        if (current->next == cmd) {
            prev_cmd = current;
        }
    }
    if (!prev_cmd) return;

    prev_cmd->next = cmd->next;
}

static struct command set_command = {
    .name = "set",
    .handler = set_handler,
    .help_message = "Set variable",
};

static void set_init(void)
{
    VlShell_RegisterCommand(&set_command);
}

REGISTER_SHELL_COMMAND(set, set_init)

static struct command help = {
    .name = "help",
    .handler = help_handler,
    .help_message = "Print this message",
};

static void help_init(void)
{
    VlShell_RegisterCommand(&help);
}

REGISTER_SHELL_COMMAND(help, help_init)

static struct command shell_command = {
    .name = "shell",
    .handler = shell_handler,
    .help_message = "Create a new instance of shell",
};

static void shell_command_init(void)
{
    VlShell_RegisterCommand(&shell_command);
}

REGISTER_SHELL_COMMAND(shell, shell_command_init)
