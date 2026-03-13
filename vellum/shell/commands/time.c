#include <vellum/shell.h>

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <vellum/device.h>
#include <vellum/interface/rtc.h>
#include <vellum/status.h>

static int time_handler(struct shell_instance *inst, int argc, char **argv)
{
    time_t timet;
    struct tm tm;

    timet = time(NULL);

    printf("UNIX timestamp: %lld\n", timet);

    gmtime_r(&timet, &tm);

    printf(
        "GMT: %d-%02d-%02d %02d:%02d:%02d\n",
        tm.tm_year + 1900,
        tm.tm_mon + 1,
        tm.tm_mday,
        tm.tm_hour,
        tm.tm_min,
        tm.tm_sec
    );

    localtime_r(&timet, &tm);

    printf(
        "Local: %d-%02d-%02d %02d:%02d:%02d\n",
        tm.tm_year + 1900,
        tm.tm_mon + 1,
        tm.tm_mday,
        tm.tm_hour,
        tm.tm_min,
        tm.tm_sec
    );

    return 0;
}

static struct command time_command = {
    .name = "time",
    .handler = time_handler,
    .help_message = "Get current time",
};

static void time_command_init(void)
{
    VlShell_RegisterCommand(&time_command);
}

REGISTER_SHELL_COMMAND(time, time_command_init)
