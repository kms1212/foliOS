#ifndef __VELLUM_INTERFACE_CONSOLE_H__
#define __VELLUM_INTERFACE_CONSOLE_H__

#include <stdint.h>
#include <wchar.h>

#include <vellum/device.h>
#include <vellum/status.h>

struct console_char_attributes {
    uint32_t fg_color : 24;
    uint32_t text_blink_level : 2;
    uint32_t text_reversed : 1;
    uint32_t text_bold : 1;
    uint32_t text_dim : 1;
    uint32_t text_italic : 1;
    uint32_t text_underline : 1;
    uint32_t text_strike : 1;

    uint32_t bg_color : 24;
    uint32_t text_overlined : 1;
    uint32_t : 7;
};

struct console_char_cell {
    struct console_char_attributes attr;
    wchar_t codepoint;
};

struct console_interface {
    VlStatus (*get_dimension)(struct device *, int *, int *);
    VlStatus (*get_buffer)(struct device *, struct console_char_cell **);
    VlStatus (*invalidate)(struct device *, int, int, int, int);
    VlStatus (*flush)(struct device *);
    VlStatus (*set_cursor_pos)(struct device *, int, int);
    VlStatus (*get_cursor_pos)(struct device *, int *, int *);
    VlStatus (*set_cursor_visibility)(struct device *, int);
    VlStatus (*get_cursor_visibility)(struct device *, int *);
    VlStatus (*set_cursor_attr)(struct device *, const void *);
    VlStatus (*get_cursor_attr)(struct device *, void *);
};

#endif  // __VELLUM_INTERFACE_CONSOLE_H__
