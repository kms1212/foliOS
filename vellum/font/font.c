#include <vellum/font.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vellum/plat/bios/video.h>

#include <vellum/compiler.h>
#include <vellum/encoding/cp437.h>

static const void *vbios_font = NULL;
static void *font_file_data = NULL;

struct font_header {
    char signature[4];
    uint32_t max_codepoint;
    uint32_t glyph_offset_table_offset;
    uint8_t reserved[4];
} __packed;

struct glyph_header {
    uint8_t is_full_width : 1;
    uint8_t : 7;
    uint8_t reserved[3];
} __packed;

status_t VlFont_Use(const char *path)
{
    long file_size;
    char signature[4];

    if (!path) {
        if (font_file_data) {
            free(font_file_data);
        }
        font_file_data = NULL;
        return STATUS_SUCCESS;
    }

    FILE *fp = fopen(path, "rb");
    if (!fp) {
        return STATUS_UNKNOWN_ERROR;
    }

    fseek(fp, 0, SEEK_END);
    file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    fread(signature, sizeof(signature), 1, fp);
    fseek(fp, 0, SEEK_SET);

    if (strncmp(signature, "bfnt", sizeof(signature)) != 0) {
        fclose(fp);
        return STATUS_INVALID_SIGNATURE;
    }

    if (font_file_data) {
        free(font_file_data);
    }
    font_file_data = malloc(file_size);
    if (!font_file_data) {
        fclose(fp);
        return STATUS_UNKNOWN_ERROR;
    }
    if (fread(font_file_data, file_size, 1, fp) != 1) {
        free(font_file_data);
        font_file_data = NULL;
    }

    fclose(fp);
    return STATUS_SUCCESS;
}

status_t VlFont_GetGlyphDimension(wchar_t codepoint, int *width, int *height)
{
    if (!font_file_data) {
        if (width) {
            *width = 8;
        }
        if (height) {
            *height = 16;
        }
    } else {
        struct font_header *header = (struct font_header *)font_file_data;
        if (codepoint > header->max_codepoint ||
            !((uint32_t *)((uint8_t *)font_file_data +
                           header->glyph_offset_table_offset))[codepoint])
            return STATUS_INVALID_VALUE;
        struct glyph_header *glyph_header =
            (struct glyph_header *)((uint8_t *)font_file_data +
                                    ((uint32_t *)((uint8_t *)font_file_data +
                                                  header->glyph_offset_table_offset))[codepoint]);

        if (width) {
            *width = glyph_header->is_full_width ? 16 : 8;
        }
        if (height) {
            *height = 16;
        }
    }

    return STATUS_SUCCESS;
}

status_t VlFont_GetGlyphData(wchar_t codepoint, uint8_t *buf, size_t size)
{
    status_t status;
    uint8_t cp437_char;
    struct font_header *header;
    uintptr_t glyph_offset;
    struct glyph_header *glyph_header;

    if (!font_file_data) {
        if (size < 16) return STATUS_INVALID_VALUE;

        status = VlEnc_Utf32ToCp437(codepoint, &cp437_char);
        if (!CHECK_SUCCESS(status)) return status;

        memcpy(buf, (const uint8_t *)vbios_font + 16 * cp437_char, 16);
    } else {
        header = (struct font_header *)font_file_data;
        glyph_offset =
            ((uint32_t *)((uintptr_t)header + header->glyph_offset_table_offset))[codepoint];
        if (codepoint > header->max_codepoint || !glyph_offset) return STATUS_INVALID_VALUE;
        glyph_header = (struct glyph_header *)((uintptr_t)header + glyph_offset);

        if (glyph_header->is_full_width && size < 32) return STATUS_INVALID_VALUE;
        if (!glyph_header->is_full_width && size < 16) return STATUS_INVALID_VALUE;

        memcpy(
            buf,
            ((uint8_t *)glyph_header) + sizeof(struct glyph_header),
            glyph_header->is_full_width ? 32 : 16
        );
    }

    return STATUS_SUCCESS;
}

__constructor static void _init_vbios_font(void)
{
    VlBiosP_GetVideoFontData(0x06, &vbios_font, NULL);
}
