#ifndef __EMOS_INTERFACE_VIDEO_H__
#define __EMOS_INTERFACE_VIDEO_H__

#include <vellum/device.h>
#include <vellum/status.h>

#define VMM_TEXT     0
#define VMM_CGA      1
#define VMM_HERCULES 2
#define VMM_PLANAR   3
#define VMM_PACKED   4
#define VMM_NONCHAIN 5
#define VMM_DIRECT   6
#define VMM_YUV      7

struct video_hw_mode_info {
    void *framebuffer;
    int width;
    int height;
    int pitch;
    int bpp;
    int memory_model;
    int rmask;
    int rpos;
    int gmask;
    int gpos;
    int bmask;
    int bpos;
};

struct video_mode_info {
    int current_mode, next_mode;
    int text;
    int width, height, bpp;
};

typedef void (*video_mode_callback_t)(void *, struct device *, int);

struct video_interface {
    status_t (*set_mode)(struct device *, int);
    status_t (*get_mode)(struct device *, int *);
    status_t (*add_mode_callback)(struct device *, void *, video_mode_callback_t, int *);
    void (*remove_mode_callback)(struct device *, int);
    status_t (*get_mode_info)(struct device *, int, struct video_mode_info *);
    status_t (*get_hw_mode_info)(struct device *, int, struct video_hw_mode_info *);
};

#endif  // __EMOS_INTERFACE_VIDEO_H__
