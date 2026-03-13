#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vellum/plat/bios/bootinfo.h>
#include <vellum/plat/bios/misc.h>
#include <vellum/plat/power.h>

#include <vellum/device.h>
#include <vellum/filesystem.h>
#include <vellum/global_configs.h>
#include <vellum/hid.h>
#include <vellum/interface/block.h>
#include <vellum/interface/char.h>
#include <vellum/interface/console.h>
#include <vellum/interface/framebuffer.h>
#include <vellum/interface/hid.h>
#include <vellum/json.h>
#include <vellum/log.h>
#include <vellum/macros.h>
#include <vellum/panic.h>
#include <vellum/shell.h>
#include <vellum/status.h>

#include <x86gprintrin.h>

#define MODULE_NAME "main"

static void setup_tty(void)
{
    status_t status;
    struct device *fbdev;
    const struct framebuffer_interface *fbi;
    struct device *condev;
    struct device_driver *condrv;
    const struct console_interface *vci;
    struct device *ttydev;
    struct device_driver *ttydrv;

    status = VlDev_Find("video0", &fbdev);
    if (!CHECK_SUCCESS(status)) {
        fprintf(stderr, "find_device() failed: 0x%08lX\n", status);
        VlP_Panic(status, "video device not available");
    }

    status = fbdev->driver->get_interface(fbdev, "framebuffer", (const void **)&fbi);
    if (!CHECK_SUCCESS(status)) {
        fprintf(stderr, "get_interface() failed: 0x%08lX\n", status);
        VlP_Panic(status, "failed to get interface from device");
    }

    status = VlDev_FindDriver("vconsole", &condrv);
    if (!CHECK_SUCCESS(status)) {
        fprintf(stderr, "find_device_driver() failed: 0x%08lX\n", status);
        VlP_Panic(status, "failed to initialize essential virtual device");
    }

    status = condrv->probe(&condev, condrv, fbdev, NULL, 0);
    if (!CHECK_SUCCESS(status)) {
        fprintf(stderr, "register_device() failed: 0x%08lX\n", status);
        VlP_Panic(status, "failed to initialize essential virtual device");
    }

    status = condev->driver->get_interface(condev, "console", (const void **)&vci);
    if (!CHECK_SUCCESS(status)) {
        fprintf(stderr, "get_interface() failed: 0x%08lX\n", status);
        VlP_Panic(status, "failed to get interface from device");
    }

    status = VlDev_FindDriver("ansiterm", &ttydrv);
    if (!CHECK_SUCCESS(status)) {
        fprintf(stderr, "find_device_driver() failed: 0x%08lX\n", status);
        VlP_Panic(status, "failed to initialize essential virtual device");
    }

    status = ttydrv->probe(&ttydev, ttydrv, condev, NULL, 0);
    if (!CHECK_SUCCESS(status)) {
        fprintf(stderr, "register_device() failed: 0x%08lX\n", status);
        VlP_Panic(status, "failed to initialize essential virtual device");
    }

    if (freopendevice("tty0", stdout)) {
        VlP_Panic(STATUS_UNKNOWN_ERROR, "failed to reopen essential file");
    }
    if (freopendevice("tty0", stderr)) {
        VlP_Panic(STATUS_UNKNOWN_ERROR, "failed to reopen essential file");
    }
    if (freopendevice("kbd0", stdin)) {
        VlP_Panic(STATUS_UNKNOWN_ERROR, "failed to reopen essential file");
    }
}

struct json_value *config_data;
const char *config_password;
time_t config_timezone_offset;
int config_rtc_utc;

static void read_config(void)
{
    status_t status;
    FILE *cfg_fp = NULL;
    long cfg_len;
    char *cfg_str = NULL;
    struct json_value *json = NULL;
    struct json_value *password = NULL;
    struct json_value *timezone = NULL;
    struct json_value *rtc_utc = NULL;
    struct json_value *start_script = NULL;

    cfg_fp = fopen("boot:/config/boot.json", "r");
    if (!cfg_fp) {
        LOG_ERROR("boot:/config/boot.json not found. using default configuration...");

        config_data = NULL;
        config_password = NULL;
        config_timezone_offset = 0;
        config_rtc_utc = 1;

        return;
    }

    fseek(cfg_fp, 0, SEEK_END);
    cfg_len = ftell(cfg_fp);
    fseek(cfg_fp, 0, SEEK_SET);

    cfg_str = malloc(cfg_len);
    if (!cfg_str) {
        VlP_Panic(STATUS_UNKNOWN_ERROR, "failed to allocate memory");
    }
    fread(cfg_str, cfg_len, 1, cfg_fp);

    status = VlJson_Parse(cfg_str, cfg_len, &json);
    if (!CHECK_SUCCESS(status) || !json || json->type != JVT_OBJECT) {
        VlP_Panic(STATUS_INVALID_FORMAT, "invalid config file");
    }

    free(cfg_str);
    fclose(cfg_fp);

    config_data = json;

    /* read out password */
    status = VlJson_GetObjectElementValue(&config_data->obj, "password", &password);
    if (!CHECK_SUCCESS(status) || !password) {
        config_password = NULL;
    } else if (password->type == JVT_STRING) {
        config_password = password->str;
    } else {
        VlP_Panic(STATUS_INVALID_FORMAT, "element \"password\" has invalid value type");
    }

    /* read out timezone */
    status = VlJson_GetObjectElementValue(&config_data->obj, "timezone", &timezone);
    if (!CHECK_SUCCESS(status) || !timezone) {
        config_timezone_offset = 0;
    } else if (timezone->type == JVT_STRING) {
        // TODO: do timezone parsing
    } else if (timezone->type == JVT_NUMBER) {
        config_timezone_offset = timezone->num;
    } else {
        VlP_Panic(STATUS_INVALID_FORMAT, "element \"timezone\" has invalid value type");
    }

    /* read out whether rtc stores utc */
    status = VlJson_GetObjectElementValue(&config_data->obj, "rtc_utc", &rtc_utc);
    if (!CHECK_SUCCESS(status) || !rtc_utc) {
        config_rtc_utc = 1; /* assume that rtc stores utc time */
    } else if (rtc_utc->type == JVT_BOOLEAN) {
        config_rtc_utc = rtc_utc->boolean;
    } else {
        VlP_Panic(STATUS_INVALID_FORMAT, "element \"rtc_utc\" has invalid value type");
    }

    /* run startup script */
    status = VlJson_GetObjectElementValue(&config_data->obj, "start_script", &start_script);
    if (!CHECK_SUCCESS(status) || !start_script) {
        // skip running startup script
    } else if (start_script->type == JVT_STRING) {
        VlShell_Execute(NULL, start_script->str);
    } else if (start_script->type == JVT_ARRAY) {
        for (struct json_array_elem *elem = start_script->arr.elem; elem; elem = elem->next) {
            if (elem->value->type != JVT_STRING) {
                VlP_Panic(
                    STATUS_INVALID_FORMAT,
                    "an element of array \"start_script\" has invalid value type"
                );
            }
            VlShell_Execute(NULL, elem->value->str);
        }
    } else {
        VlP_Panic(STATUS_INVALID_FORMAT, "element \"start_script\" has invalid value type");
    }
}

static int show_menu(struct json_value *menu, int root_menu)
{
    status_t status;
    struct device *kbd;
    const struct hid_interface *hidi;
    struct json_value *title = NULL;
    char *title_value;
    struct json_value *timeout = NULL;
    int timeout_value;
    struct json_value *_default = NULL;
    int default_value;
    struct json_value *options = NULL;
    unsigned int option_count;
    int selection, selected, selection_changed;
    uint16_t key, flags;
    struct json_value *option = NULL;
    struct json_value *option_name = NULL;
    struct json_value *option_script = NULL;
    struct json_value *submenu = NULL;

    /* read out title */
    status = VlJson_GetObjectElementValue(&menu->obj, "title", &title);
    if (!CHECK_SUCCESS(status) || !title) {
        title_value = NULL;
    } else if (title->type == JVT_STRING) {
        title_value = title->str;
    } else {
        VlP_Panic(STATUS_INVALID_FORMAT, "element \"title\" has invalid value type");
    }

    /* read out option selection timeout */
    status = VlJson_GetObjectElementValue(&menu->obj, "timeout", &timeout);
    if (!CHECK_SUCCESS(status) || !timeout) {
        timeout_value = 5;
    } else if (timeout->type == JVT_NUMBER) {
        timeout_value = (int)timeout->num;
    } else {
        VlP_Panic(STATUS_INVALID_FORMAT, "element \"timeout\" has invalid value type");
    }

    /* read out default option entry */
    status = VlJson_GetObjectElementValue(&menu->obj, "default", &_default);
    if (!CHECK_SUCCESS(status) || !_default) {
        default_value = 0;
    } else if (_default->type == JVT_NUMBER) {
        default_value = (int)_default->num;
    } else {
        VlP_Panic(STATUS_INVALID_FORMAT, "element \"default\" has invalid value type");
    }

    status = VlDev_Find("kbd0", &kbd);
    if (!CHECK_SUCCESS(status)) {
        VlP_Panic(status, "keyboard not detected");
    }

    status = kbd->driver->get_interface(kbd, "hid", (const void **)&hidi);
    if (!CHECK_SUCCESS(status)) {
        VlP_Panic(status, "failed to get interface from device");
    }

    status = VlJson_GetObjectElementValue(&menu->obj, "options", &options);
    if (!CHECK_SUCCESS(status) || !options) {
        VlP_Panic(
            !CHECK_SUCCESS(status) ? status : STATUS_INVALID_FORMAT,
            "element \"options\" not found"
        );
    } else if (options->type != JVT_ARRAY) {
        VlP_Panic(STATUS_INVALID_FORMAT, "element \"options\" has invalid value type");
    }

    status = VlJson_GetArrayElementCount(&options->arr, &option_count);
    if (!CHECK_SUCCESS(status)) {
        VlP_Panic(status, "cannot get element count of element \"options\"");
    }

reselect:
    if (title_value) {
        printf(" \x1b[3J\x1b[0;0f\x1b[?25l\n  %s\n  ", title_value);
        for (int i = 0; title_value[i]; i++) {
            fputs("═", stdout);
        }
    }

    selected = 0;
    selection = default_value;
    selection_changed = 0;
    while (!selected) {
        printf("\x1b[?25l\x1b[5;0H");
        for (int i = 0; i < option_count; i++) {
            status = VlJson_GetArrayElementValue(&options->arr, i, &option);
            if (!CHECK_SUCCESS(status) || !option || option->type != JVT_OBJECT) {
                VlP_Panic(
                    STATUS_INVALID_FORMAT,
                    "an element of array \"options\" is not found or has invalid type"
                );
            }

            status = VlJson_GetObjectElementValue(&option->obj, "name", &option_name);
            if (!CHECK_SUCCESS(status) || !option_name || option_name->type != JVT_STRING) {
                VlP_Panic(
                    STATUS_INVALID_FORMAT,
                    "element \"name\" is not found or has invalid type"
                );
            }

            if (selection == i) {
                printf("  \x1b[7m%d. %s\x1b[0m\n", i + 1, option_name->str);
            } else {
                printf("  %d. %s\n", i + 1, option_name->str);
            }
        }

        if (!root_menu) {
            if (selection == option_count) {
                printf("  \x1b[7m%d. Go Back\x1b[0m\n", option_count + 1);
            } else {
                printf("  %d. Go Back\n", option_count + 1);
            }
        }

        if (selection_changed) {
            printf(
                "\n  Select option: %d\x1b[0K\x1b[%d;18H\x1b[?25h",
                selection + 1,
                root_menu ? option_count + 6 : option_count + 7
            );
        } else {
            printf(
                "\n  Select option: %d\tTime remaining: %d\x1b[%d;18H\x1b[?25h",
                selection + 1,
                timeout_value,
                root_menu ? option_count + 6 : option_count + 7
            );
        }

        status = hidi->wait_event(kbd);
        if (!CHECK_SUCCESS(status)) continue;

        do {
            status = hidi->poll_event(kbd, &key, &flags);
        } while (status == STATUS_BUFFER_UNDERFLOW);
        if (!CHECK_SUCCESS(status) || (flags & 1)) continue;

        switch (key) {
        case KEY_UP:
            if (selection > 0) {
                selection--;
            }
            selection_changed = 1;
            break;
        case KEY_DOWN:
            if (selection < (root_menu ? option_count - 1 : option_count)) {
                selection++;
            }
            selection_changed = 1;
            break;
        case KEY_ENTER:
        case KEY_KPENTER:
            selected = 1;
            break;
        default:
            break;
        }
    }

    if (selection == option_count) {
        return 1;
    }

    status = VlJson_GetArrayElementValue(&options->arr, selection, &option);
    if (!CHECK_SUCCESS(status) || !option || option->type != JVT_OBJECT) {
        VlP_Panic(
            STATUS_INVALID_FORMAT,
            "an element of array \"options\" is not found or has invalid type"
        );
    }

    fputs("\x1b[3J\x1b[0;0f\x1b[?25h", stdout);

    status = VlJson_GetObjectElementValue(&option->obj, "script", &option_script);
    if (!CHECK_SUCCESS(status) || !option_script) {
    } else if (option_script->type == JVT_STRING) {
        VlShell_Execute(NULL, option_script->str);
    } else if (option_script->type == JVT_ARRAY) {
        for (struct json_array_elem *elem = option_script->arr.elem; elem; elem = elem->next) {
            if (elem->value->type != JVT_STRING) {
                VlP_Panic(
                    STATUS_INVALID_FORMAT,
                    "an element of array \"script\" has invalid value type"
                );
            }
            VlShell_Execute(NULL, elem->value->str);
        }
    } else {
        VlP_Panic(STATUS_INVALID_FORMAT, "element \"script\" has invalid value type");
    }

    status = VlJson_GetObjectElementValue(&option->obj, "submenu", &submenu);
    if (!CHECK_SUCCESS(status) || !submenu) {
    } else if (submenu->type == JVT_OBJECT) {
        if (show_menu(submenu, 0)) goto reselect;
    } else {
        VlP_Panic(STATUS_INVALID_FORMAT, "element \"script\" has invalid value type");
    }

    return 0;
}

__noreturn void main(void)
{
    status_t status;
    struct json_value *menu = NULL;

    setup_tty();

    read_config();

    if (config_data) {
        status = VlJson_GetObjectElementValue(&config_data->obj, "menu", &menu);
        if (!CHECK_SUCCESS(status) || !menu) {
        } else if (menu->type == JVT_OBJECT) {
            show_menu(menu, 1);
        } else {
            VlP_Panic(STATUS_INVALID_FORMAT, "element \"menu\" has invalid value type");
        }
    } else {
        printf("boot configuration file not found.\n");
    }

    for (;;) {
        VlShell_Execute(NULL, "shell");
    }
}
