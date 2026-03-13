#include <vellum/shell.h>

#include <stdio.h>

#include <vellum/device.h>

static void print_dev_tree(struct device *parent_dev, int indent, uint32_t chain)
{
    struct device *dev = parent_dev->first_child;

    while (dev) {
        for (int i = 0; i < indent; i++) {
            printf(((chain >> i) & 1) ? "│   " : "    ");
        }
        printf(dev->sibling ? "├───" : "└───");
        printf("%s: %s\n", dev->name, dev->driver->name);

        if (dev->first_child) {
            print_dev_tree(dev, indent + 1, chain | (dev->sibling ? (1 << indent) : 0));
        }

        dev = dev->sibling;
    }
}

static int lsdev_handler(struct shell_instance *inst, int argc, char **argv)
{
    struct device *dev = VlDev_GetFirst();
    struct device *last_root_dev = NULL;

    printf("System\n");

    while (dev) {
        if (!dev->parent) {
            if (last_root_dev) {
                printf("├───%s: %s\n", last_root_dev->name, last_root_dev->driver->name);
                print_dev_tree(last_root_dev, 1, 1);
            }

            last_root_dev = dev;
        }

        dev = dev->next;
    }

    if (last_root_dev) {
        printf("└───%s: %s\n", last_root_dev->name, last_root_dev->driver->name);
        print_dev_tree(last_root_dev, 1, 0);
    }

    return 0;
}

static struct command lsdev_command = {
    .name = "lsdev",
    .handler = lsdev_handler,
    .help_message = "List devices",
};

static void lsdev_command_init(void)
{
    VlShell_RegisterCommand(&lsdev_command);
}

REGISTER_SHELL_COMMAND(lsdev, lsdev_command_init)
