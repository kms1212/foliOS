#include <stdio.h>

#include <vellum/compiler.h>
#include <vellum/shell.h>
#include <vellum/status.h>

static int helloworld_handler(struct shell_instance *inst, int argc, char **argv)
{
    printf("Hello, World!\n");

    return 0;
}

static struct command helloworld_command = {
    .name = "helloworld",
    .handler = helloworld_handler,
    .help_message = "Run BASIC interpreter",
};

__constructor static void init()
{
    VlShell_RegisterCommand(&helloworld_command);
}

VlStatus _start(int argc, char **argv)
{
    return STATUS_SUCCESS;
}

__destructor static void deinit(void)
{
    VlShell_UnregisterCommand(&helloworld_command);
}
