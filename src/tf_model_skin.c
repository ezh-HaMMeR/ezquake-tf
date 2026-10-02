#include "tf_model_skin.h"
#include "tf_model_skin_masks.h"
#include <string.h>
#include <zlib.h>

int TF_ModelSkinTeam(const char *name)
{
	const char *suffix;
	if (!strncmp(name, "progs/tfbody", 12)) suffix = name + 12;
	else if (!strncmp(name, "progs/tfheadless", 16)) suffix = name + 16;
	else return 0;
	return suffix[0] >= '1' && suffix[0] <= '4' && !strcmp(suffix + 1, ".mdl") ? suffix[0] - '0' : 0;
}

int TF_ModelSkinHeadless(const char *name)
{
	return !strncmp(name, "progs/tfheadless", 16);
}

int TF_ModelSkinMask(int headless, int skin, int width, int height, unsigned char *mask)
{
	unsigned int i, end;
	uLongf size = TF_MASK_WIDTH * TF_MASK_HEIGHT;
	if (skin < 0 || skin >= 10 || width != TF_MASK_WIDTH || height != TF_MASK_HEIGHT) return 0;
	i = tf_mask_offsets[(headless ? 10 : 0) + skin];
	end = tf_mask_offsets[(headless ? 10 : 0) + skin + 1];
	return uncompress(mask, &size, tf_mask_data + i, end - i) == Z_OK && size == TF_MASK_WIDTH * TF_MASK_HEIGHT;
}

void TF_ModelSkinTranslate(const unsigned char *original, const unsigned char *mask,
	size_t count, const unsigned int palette[256], const tf_skin_color_t colors[2], unsigned int *pixels)
{
	unsigned int ramps[2][16];
	int range, i, channel;
	size_t p;
	for (range = 0; range < 2; ++range) {
		int start = range ? 96 : 16;
		int peak = 1;
		const tf_skin_color_t *color = &colors[range];
		for (i = 0; i < 16; ++i) {
			const unsigned char *shade = (const unsigned char *)&palette[start + i];
			for (channel = 0; channel < 3; ++channel) if (shade[channel] > peak) peak = shade[channel];
		}
		for (i = 0; i < 16; ++i) {
			int pal = color->palette < 0 ? 0 : color->palette > 13 ? 13 : color->palette;
			ramps[range][i] = palette[pal * 16 + (pal < 8 ? i : 15 - i)];
			if (color->rgb) {
				const unsigned char *shade = (const unsigned char *)&palette[start + i];
				unsigned char *dst = (unsigned char *)&ramps[range][i];
				int brightness = shade[0];
				if (shade[1] > brightness) brightness = shade[1];
				if (shade[2] > brightness) brightness = shade[2];
				for (channel = 0; channel < 3; ++channel) dst[channel] = color->color[channel] * brightness / peak;
				dst[3] = 255;
			}
		}
	}
	for (p = 0; p < count; ++p) {
		int index = mask[p];
		pixels[p] = palette[original[p]];
		if (index >= 16 && index < 32 && colors[0].enabled) pixels[p] = ramps[0][index - 16];
		else if (index >= 96 && index < 112 && colors[1].enabled) pixels[p] = ramps[1][index - 96];
	}
}
