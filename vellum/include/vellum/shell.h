#ifndef __VELLUM_SHELL_H__
#define __VELLUM_SHELL_H__

#include <stddef.h>

#include <vellum/compiler.h>
#include <vellum/status.h>

struct shell_var {
    struct shell_var *next;
    size_t key_len;
    size_t value_len;
    char str[];
};

struct shell_instance {
    struct filesystem *fs;
    struct fs_directory *working_dir;
    char working_dir_path[256];
    struct shell_var *var_list;
};

struct command {
    struct command *next;

    const char *name;
    int (*handler)(struct shell_instance *inst, int argc, char **argv);
    const char *help_message;
};

void shell_start(void);
long shell_readline(const char *prompt, char *buf, long len);
status_t shell_expand(struct shell_instance *inst, const char *line, char *buf, long buflen);
const char *shell_parse(const char *line, char *buf, long buflen);
int shell_execute(struct shell_instance *inst, const char *line);

status_t shell_get_variable(struct shell_instance *inst, const char *key, const char **value);
status_t shell_set_variable(struct shell_instance *inst, const char *key, const char *value);
status_t shell_remove_variable(struct shell_instance *inst, const char *key);

status_t shell_command_register(struct command *cmd);
void shell_command_unregister(struct command *cmd);

#define REGISTER_SHELL_COMMAND(name, init_func)                                                    \
    __constructor static void _register_driver_##name(void)                                        \
    {                                                                                              \
        init_func();                                                                               \
    }

#endif  // __VELLUM_SHELL_H__
