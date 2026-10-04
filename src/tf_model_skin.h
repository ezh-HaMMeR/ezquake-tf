#ifndef TF_MODEL_SKIN_H
#define TF_MODEL_SKIN_H

#include <stddef.h>
#define TF_MODEL_SKIN_WIDTH 296
#define TF_MODEL_SKIN_HEIGHT 194

typedef struct {
	int enabled, palette, rgb;
	unsigned char color[3];
} tf_skin_color_t;

/* Model identity is authoritative, including corpses without a player slot. */
int TF_ModelSkinTeam(const char *name);
int TF_ModelSkinKind(const char *name);
int TF_ModelSkinDimensions(int kind, int width, int height);
int TF_ModelSkinMask(int headless, int skin, int width, int height, unsigned char *mask);
void TF_ModelSkinTranslate(const unsigned char *original, const unsigned char *mask,
	size_t count, const unsigned int palette[256], const tf_skin_color_t colors[2],
	unsigned int *pixels);

#endif
