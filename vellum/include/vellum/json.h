#ifndef __VELLUM_JSON_H__
#define __VELLUM_JSON_H__

#include <stdint.h>

#include <vellum/compiler.h>
#include <vellum/status.h>

enum json_value_type {
    JVT_NULL = 0,
    JVT_STRING,
    JVT_NUMBER,
    JVT_BOOLEAN,
    JVT_OBJECT,
    JVT_ARRAY,
};

struct json_object {
    struct json_object_elem *elem;
};

struct json_array {
    struct json_array_elem *elem;
};

struct json_value {
    enum json_value_type type;
    union {
        struct json_object obj;
        struct json_array arr;
        int64_t num;
        int boolean;
        char *str;
    };
};

struct json_object_elem {
    struct json_object_elem *next;
    char *key;
    struct json_value *value;
};

struct json_array_elem {
    struct json_array_elem *next;
    unsigned int index;
    struct json_value *value;
};

void json_destruct(struct json_value *obj);

status_t json_parse(const char *str, long len, struct json_value **json);

status_t json_object_find_value(struct json_object *obj, const char *str, struct json_value **value);
status_t json_array_find_value(struct json_array *arr, unsigned int idx, struct json_value **value);
status_t json_array_get_element_count(struct json_array *arr, unsigned int *count);

#endif // __VELLUM_JSON_H__
