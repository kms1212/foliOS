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

void VlShell_Start(void);
long VlShell_Readline(const char *prompt, char *buf, long len);
status_t VlShell_Expand(struct shell_instance *inst, const char *line, char *buf, long buflen);
const char *VlShell_Parse(const char *line, char *buf, long buflen);
int VlShell_Execute(struct shell_instance *inst, const char *line);

status_t VlShell_GetVariable(struct shell_instance *inst, const char *key, const char **value);
status_t VlShell_SetVariable(struct shell_instance *inst, const char *key, const char *value);
status_t VlShell_RemoveVariable(struct shell_instance *inst, const char *key);

status_t VlShell_RegisterCommand(struct command *cmd);
void VlShell_UnregisterCommand(struct command *cmd);

#define REGISTER_SHELL_COMMAND(name, init_func)                                                    \
    __constructor static void _register_driver_##name(void)                                        \
    {                                                                                              \
        init_func();                                                                               \
    }

#endif  // __VELLUM_SHELL_H__
