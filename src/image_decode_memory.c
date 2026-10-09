#include "image_decode_memory.h"
#include <stdlib.h>
#include <string.h>
#include <png.h>

#define DECODE_MAX_DIMENSION 4096

static unsigned ReadBE32(const unsigned char *p)
{
    return (unsigned)p[0] << 24 | (unsigned)p[1] << 16 | (unsigned)p[2] << 8 | p[3];
}

size_t Image_MemoryDecodeSize(const unsigned char *data, size_t length, int png)
{
    unsigned w, h;
    if (!data) return 0;
    if (png) {
        if (length < 33 || png_sig_cmp(data, 0, 8) ||
            ReadBE32(data + 8) != 13 || memcmp(data + 12, "IHDR", 4)) return 0;
        w = ReadBE32(data + 16); h = ReadBE32(data + 20);
    }
    else {
        if (length < 18 || data[1] || (data[2] != 2 && data[2] != 10) ||
            (data[16] != 24 && data[16] != 32) || (data[17] & 0x10)) return 0;
        w = data[12] | (unsigned)data[13] << 8;
        h = data[14] | (unsigned)data[15] << 8;
    }
    if (!w || !h || w > DECODE_MAX_DIMENSION || h > DECODE_MAX_DIMENSION) return 0;
    return (size_t)w * h * 4;
}

typedef struct {
    const unsigned char *data;
    size_t length, offset;
    unsigned char *pixels;
    png_bytep *rows;
} png_memory_t;

static void DecodeError(png_structp png, png_const_charp message)
{
    (void)message;
    png_longjmp(png, 1);
}

static void DecodeWarning(png_structp png, png_const_charp message)
{
    (void)png; (void)message;
}

static void DecodeRead(png_structp png, png_bytep output, png_size_t count)
{
    png_memory_t *ctx = png_get_io_ptr(png);
    if (count > ctx->length - ctx->offset) png_error(png, "Truncated PNG");
    memcpy(output, ctx->data + ctx->offset, count);
    ctx->offset += count;
}

static unsigned char *DecodePNG(const unsigned char *data, size_t length, int *width, int *height)
{
    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, DecodeError, DecodeWarning);
    png_infop info;
    png_memory_t *ctx;
    unsigned char *result;
    png_uint_32 w, h, y;
    int depth, color;
    if (!png) return NULL;
    info = png_create_info_struct(png);
    ctx = calloc(1, sizeof(*ctx));
    if (!info || !ctx) {
        png_destroy_read_struct(&png, info ? &info : NULL, NULL);
        free(ctx);
        return NULL;
    }
    /* Cleanup state lives on the heap: setjmp must not invalidate it. */
    if (setjmp(png_jmpbuf(png))) {
        free(ctx->pixels); free(ctx->rows); free(ctx);
        png_destroy_read_struct(&png, &info, NULL);
        return NULL;
    }
    ctx->data = data; ctx->length = length;
    png_set_read_fn(png, ctx, DecodeRead);
    png_set_user_limits(png, DECODE_MAX_DIMENSION, DECODE_MAX_DIMENSION);
    png_read_info(png, info);
    w = png_get_image_width(png, info); h = png_get_image_height(png, info);
    depth = png_get_bit_depth(png, info); color = png_get_color_type(png, info);
    /* Match Image_LoadPNG_All, including palette transparency and 16-bit truncation. */
    if (color == PNG_COLOR_TYPE_PALETTE) {
        png_set_palette_to_rgb(png); png_set_filler(png, 255, PNG_FILLER_AFTER);
    }
    if (color == PNG_COLOR_TYPE_GRAY && depth < 8) png_set_expand_gray_1_2_4_to_8(png);
    if (png_get_valid(png, info, PNG_INFO_tRNS)) png_set_tRNS_to_alpha(png);
    if (color == PNG_COLOR_TYPE_GRAY || color == PNG_COLOR_TYPE_GRAY_ALPHA) png_set_gray_to_rgb(png);
    if (color != PNG_COLOR_TYPE_RGBA) png_set_filler(png, 255, PNG_FILLER_AFTER);
    if (depth < 8) png_set_expand(png);
    else if (depth == 16) png_set_strip_16(png);
    png_read_update_info(png, info);
    if (png_get_rowbytes(png, info) != (size_t)w * 4 || png_get_bit_depth(png, info) != 8)
        png_error(png, "Unsupported output layout");
    ctx->pixels = malloc((size_t)w * h * 4);
    ctx->rows = malloc((size_t)h * sizeof(*ctx->rows));
    if (!ctx->pixels || !ctx->rows) png_error(png, "Out of memory");
    for (y = 0; y < h; ++y) ctx->rows[y] = ctx->pixels + (size_t)y * w * 4;
    png_read_image(png, ctx->rows);
    png_read_end(png, info);
    result = ctx->pixels;
    *width = (int)w; *height = (int)h;
    free(ctx->rows); free(ctx);
    png_destroy_read_struct(&png, &info, NULL);
    return result;
}

static unsigned char *DecodeTGA(const unsigned char *data, size_t length, int *width, int *height)
{
    size_t size = Image_MemoryDecodeSize(data, length, 0), offset, pixel = 0;
    unsigned char *out;
    unsigned w, h, bpp, x = 0, y = 0;
    if (!size) return NULL;
    w = data[12] | (unsigned)data[13] << 8; h = data[14] | (unsigned)data[15] << 8;
    bpp = data[16] / 8; offset = 18 + data[0];
    if (offset > length) return NULL;
    out = malloc(size);
    if (!out) return NULL;
    while (pixel < size / 4) {
        unsigned run = 1, repeat = 0, j;
        if (data[2] == 10) {
            if (offset == length) goto fail;
            repeat = data[offset] & 128; run = (data[offset++] & 127) + 1;
        }
        if (run > size / 4 - pixel || (size_t)bpp * (repeat ? 1 : run) > length - offset) goto fail;
        for (j = 0; j < run; ++j, ++pixel) {
            size_t row = y;
            unsigned char *p;
            const unsigned char *src = data + offset + (repeat ? 0 : j * bpp);
            if (!(data[17] & 32)) row = h - 1 - row;
            p = out + (row * w + x) * 4;
            p[0] = src[2]; p[1] = src[1]; p[2] = src[0]; p[3] = bpp == 4 ? src[3] : 255;
            if (++x == w) { x = 0; ++y; }
        }
        offset += (size_t)bpp * (repeat ? 1 : run);
    }
    *width = (int)w; *height = (int)h;
    return out;
fail:
    free(out);
    return NULL;
}

unsigned char *Image_DecodeMemory(const unsigned char *data, size_t length, int png, int *width, int *height)
{
    if (!data || !width || !height || !Image_MemoryDecodeSize(data, length, png)) return NULL;
    return png ? DecodePNG(data, length, width, height) : DecodeTGA(data, length, width, height);
}
