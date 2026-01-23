#ifndef __TYPES_H__
#define __TYPES_H__

struct point2 {
    int x, y;
};

#define POINT2(x, y) ((struct point2){ (x), (y) })

struct size2 {
    int width, height;
};

#define SIZE2(w, h) ((struct size2){ (w), (h) })

struct rect2 {
    int x, y, width, height;
};

#define RECT2(x, y, w, h) ((struct rect2){ (x), (y), (w), (h) })
#define RECT2_GET_POINT2(rect) ((struct point2){ (rect).x, (rect).y })
#define RECT2_GET_SIZE2(rect) ((struct size2){ (rect).width, (rect).height })

#endif // __TYPES_H__
