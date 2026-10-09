#ifndef EZ_IMAGE_DECODE_MEMORY_H
#define EZ_IMAGE_DECODE_MEMORY_H
#include <stddef.h>

/* No engine globals, VFS, engine allocator or renderer calls. Safe on workers.
 * The returned RGBA8 buffer belongs to the caller and must be free()'d. */
size_t Image_MemoryDecodeSize(const unsigned char *data, size_t length, int png);
unsigned char *Image_DecodeMemory(const unsigned char *data, size_t length,
    int png, int *width, int *height);
#endif
