#include <vellum/json.h>

#include <ctype.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <vellum/panic.h>

enum token_type {
    TOKEN_ERR = 0,
    TOKEN_STRING = 0x100,
    TOKEN_INTEGER,
    TOKEN_REAL,
    TOKEN_TRUE,
    TOKEN_FALSE,
    TOKEN_NULL,
};

struct json_state {
    const char *str;
    long len, cursor;
};

static status_t parse_string_literal(struct json_state *state, char **ptr);
static status_t parse_string(struct json_state *state, struct json_value **valueout);
static status_t parse_value(struct json_state *state, struct json_value **valueout);

static void skip_whitespace(struct json_state *state)
{
    for (; state->cursor < state->len && isspace(state->str[state->cursor]); state->cursor++) {
    }
}

static status_t parse_object(struct json_state *state, struct json_value **valueout)
{
    status_t status;

    if (!valueout) return STATUS_INVALID_VALUE;

    skip_whitespace(state);

    if (state->str[state->cursor] != '{') return STATUS_INVALID_SYNTAX;
    if (++state->cursor >= state->len) return STATUS_INVALID_SYNTAX;

    struct json_value *value = malloc(sizeof(struct json_value));
    if (!value) {
        status = STATUS_UNKNOWN_ERROR;
        goto has_error;
    }
    value->type = JVT_OBJECT;

    struct json_object_elem **current_elem = &value->obj.elem;

    int end = 0;
    while (!end) {
        skip_whitespace(state);

        *current_elem = malloc(sizeof(struct json_object_elem));
        if (!*current_elem) {
            status = STATUS_UNKNOWN_ERROR;
            goto has_error;
        }
        (*current_elem)->next = NULL;

        status = parse_string_literal(state, &(*current_elem)->key);
        if (!CHECK_SUCCESS(status)) goto has_error;

        skip_whitespace(state);

        if (state->str[state->cursor] != ':') {
            status = STATUS_SYNTAX_ERROR;
            goto has_error;
        }
        if (++state->cursor >= state->len) {
            status = STATUS_SYNTAX_ERROR;
            goto has_error;
        }

        skip_whitespace(state);

        status = parse_value(state, &(*current_elem)->value);
        if (!CHECK_SUCCESS(status)) goto has_error;

        skip_whitespace(state);

        switch (state->str[state->cursor]) {
        case '}':
            end = 1;
            break;
        case ',':
            if (++state->cursor >= state->len) {
                status = STATUS_SYNTAX_ERROR;
                goto has_error;
            }
            current_elem = &(*current_elem)->next;
            break;
        default:
            status = STATUS_SYNTAX_ERROR;
            goto has_error;
        }
        if (++state->cursor >= state->len) {
            status = STATUS_SYNTAX_ERROR;
            goto has_error;
        }
    }

    *valueout = value;

    return STATUS_SUCCESS;

has_error:
    // TODO: cleanup
    if (value) {
        free(value);
    }

    return status;
}

static status_t parse_array(struct json_state *state, struct json_value **valueout)
{
    status_t status;
    struct json_value *value = NULL;

    if (!valueout) return STATUS_INVALID_VALUE;

    skip_whitespace(state);

    if (state->str[state->cursor] != '[') {
        status = STATUS_INVALID_SYNTAX;
        goto has_error;
    }
    if (++state->cursor >= state->len) {
        status = STATUS_INVALID_SYNTAX;
        goto has_error;
    }

    value = malloc(sizeof(struct json_value));
    if (!value) {
        status = STATUS_UNKNOWN_ERROR;
        goto has_error;
    }
    value->type = JVT_ARRAY;

    struct json_array_elem **current_elem = &value->arr.elem;

    int end = 0;
    unsigned int index = 0;
    while (!end) {
        skip_whitespace(state);

        *current_elem = malloc(sizeof(struct json_array_elem));
        if (!*current_elem) {
            status = STATUS_UNKNOWN_ERROR;
            goto has_error;
        }
        (*current_elem)->next = NULL;
        (*current_elem)->index = index++;

        status = parse_value(state, &(*current_elem)->value);
        if (!CHECK_SUCCESS(status)) goto has_error;

        skip_whitespace(state);

        switch (state->str[state->cursor]) {
        case ']':
            end = 1;
            break;
        case ',':
            if (++state->cursor >= state->len) {
                status = STATUS_INVALID_SYNTAX;
                goto has_error;
            }
            current_elem = &(*current_elem)->next;
            break;
        default:
            status = STATUS_INVALID_SYNTAX;
            goto has_error;
        }
        if (++state->cursor >= state->len) {
            status = STATUS_INVALID_SYNTAX;
            goto has_error;
        }
    }

    *valueout = value;

    return STATUS_SUCCESS;

has_error:
    // TODO: cleanup
    if (value) {
        free(value);
    }

    return status;
}

static size_t count_escaped_string_len(struct json_state *state)
{
    long len = 0;
    for (long i = 0; state->cursor + i < state->len; i++) {
        switch (state->str[state->cursor + i]) {
        case '\\':
            i++;
            break;
        case '"':
            goto end;
        default:
            break;
        }

        len++;
    }
end:

    return len;
}

static status_t parse_string_literal(struct json_state *state, char **ptr)
{
    status_t status;
    char *str = NULL;

    if (!ptr) return STATUS_INVALID_VALUE;

    skip_whitespace(state);

    if (state->str[state->cursor] != '"') {
        status = STATUS_INVALID_SYNTAX;
        goto has_error;
    }
    if (++state->cursor >= state->len) {
        status = STATUS_INVALID_SYNTAX;
        goto has_error;
    }

    size_t string_len = count_escaped_string_len(state);
    str = malloc(string_len + 1);
    if (!str) {
        status = STATUS_UNKNOWN_ERROR;
        goto has_error;
    }
    size_t bufcur = 0;

    int end = 0;
    while (!end) {
        if (state->str[state->cursor] == '\\') {
            if (++state->cursor >= state->len) {
                status = STATUS_INVALID_SYNTAX;
                goto has_error;
            }
        } else if (state->str[state->cursor] == '"') {
            end = 1;
        } else {
            str[bufcur++] = state->str[state->cursor];
        }

        if (++state->cursor >= state->len) {
            status = STATUS_INVALID_SYNTAX;
            goto has_error;
        }
    }
    str[bufcur] = '\0';

    *ptr = str;

    return STATUS_SUCCESS;

has_error:
    // TODO: cleanup
    if (str) {
        free(str);
    }

    return status;
}

static status_t parse_string(struct json_state *state, struct json_value **valueout)
{
    status_t status;
    struct json_value *value = NULL;

    if (!valueout) return STATUS_INVALID_VALUE;

    if (state->str[state->cursor] != '"') {
        status = STATUS_INVALID_SYNTAX;
        goto has_error;
    }

    value = malloc(sizeof(struct json_value));
    if (!value) {
        status = STATUS_UNKNOWN_ERROR;
        goto has_error;
    }
    value->type = JVT_STRING;

    status = parse_string_literal(state, &value->str);
    if (!CHECK_SUCCESS(status)) goto has_error;

    *valueout = value;

    return STATUS_SUCCESS;

has_error:
    // TODO: cleanup
    if (value) {
        free(value);
    }

    return status;
}

static status_t parse_number(struct json_state *state, struct json_value **valueout)
{
    status_t status;

    if (!valueout) return STATUS_INVALID_VALUE;

    skip_whitespace(state);

    if (!isdigit(state->str[state->cursor]) && state->str[state->cursor] != '-')
        return STATUS_INVALID_SYNTAX;
    if (state->cursor + 1 >= state->len) return STATUS_INVALID_SYNTAX;

    struct json_value *value = malloc(sizeof(struct json_value));
    if (!value) {
        status = STATUS_UNKNOWN_ERROR;
        goto has_error;
    }
    value->type = JVT_NUMBER;

    int negate = 0;
    if (state->str[state->cursor] == '-') {
        negate = 1;
        state->cursor++;
    }

    value->num = 0;
    for (; isdigit(state->str[state->cursor]); state->cursor++) {
        value->num = value->num * 10 + state->str[state->cursor] - '0';
    }

    if (negate) {
        value->num = -value->num;
    }

    *valueout = value;

    return STATUS_SUCCESS;

has_error:
    // TODO: cleanup

    return status;
}

static status_t parse_other(struct json_state *state, struct json_value **valueout)
{
    status_t status;

    if (!valueout) return STATUS_INVALID_VALUE;

    skip_whitespace(state);

    struct json_value *value = malloc(sizeof(struct json_value));
    if (!value) {
        status = STATUS_UNKNOWN_ERROR;
        goto has_error;
    }

    switch (state->str[state->cursor]) {
    case 't':
        if (state->cursor + 4 >= state->len) {
            status = STATUS_INVALID_SYNTAX;
            goto has_error;
        }
        if (strncmp("true", state->str + state->cursor, 4) != 0) {
            status = STATUS_INVALID_SYNTAX;
            goto has_error;
        }
        state->cursor += 4;
        value->type = JVT_BOOLEAN;
        value->boolean = 1;
        break;
    case 'f':
        if (state->cursor + 5 >= state->len) {
            status = STATUS_INVALID_SYNTAX;
            goto has_error;
        }
        if (strncmp("false", state->str + state->cursor, 5) != 0) {
            status = STATUS_INVALID_SYNTAX;
            goto has_error;
        }
        state->cursor += 5;
        value->type = JVT_BOOLEAN;
        value->boolean = 0;
        break;
    case 'n':
        if (state->cursor + 4 >= state->len) {
            status = STATUS_INVALID_SYNTAX;
            goto has_error;
        }
        if (strncmp("null", state->str + state->cursor, 4) != 0) {
            status = STATUS_INVALID_SYNTAX;
            goto has_error;
        }
        state->cursor += 4;
        value->type = JVT_NULL;
        break;
    default:
        status = STATUS_INVALID_SYNTAX;
        goto has_error;
    }

    *valueout = value;

    return STATUS_SUCCESS;

has_error:
    // TODO: cleanup
    if (value) {
        free(value);
    }

    return status;
}

static status_t parse_value(struct json_state *state, struct json_value **value)
{
    skip_whitespace(state);

    switch (state->str[state->cursor]) {
    case '{':
        return parse_object(state, value);
    case '[':
        return parse_array(state, value);
    case '"':
        return parse_string(state, value);
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
    case '-':
        return parse_number(state, value);
    case 't':
    case 'f':
    case 'n':
        return parse_other(state, value);
    default:
        return STATUS_INVALID_SYNTAX;
    }
}

status_t VlJson_Parse(const char *str, long len, struct json_value **valueout)
{
    struct json_state state;

    if (!str || !valueout) return STATUS_INVALID_VALUE;

    state.str = str;
    state.len = len;
    state.cursor = 0;

    struct json_value *value;

    status_t status = parse_value(&state, &value);
    if (!CHECK_SUCCESS(status)) return status;

    *valueout = value;

    return STATUS_SUCCESS;
}

void VlJson_Destruct(struct json_value *json)
{
    free(json);
}

status_t VlJson_GetObjectElementValue(
    struct json_object *obj, const char *str, struct json_value **valueout
)
{
    for (struct json_object_elem *elem = obj->elem; elem; elem = elem->next) {
        if (strcmp(str, elem->key) == 0) {
            if (valueout) *valueout = elem->value;
            return STATUS_SUCCESS;
        }
    }

    return STATUS_ENTRY_NOT_FOUND;
}

status_t VlJson_GetArrayElementValue(
    struct json_array *arr, unsigned int idx, struct json_value **valueout
)
{
    for (struct json_array_elem *elem = arr->elem; elem; elem = elem->next) {
        if (elem->index == idx) {
            if (valueout) *valueout = elem->value;
            return STATUS_SUCCESS;
        }
    }

    return STATUS_ENTRY_NOT_FOUND;
}

status_t VlJson_GetArrayElementCount(struct json_array *arr, unsigned int *countout)
{
    unsigned int count = 0;

    for (struct json_array_elem *elem = arr->elem; elem; elem = elem->next) {
        count++;
    }

    if (countout) *countout = count;

    return STATUS_SUCCESS;
}
