#include "tf_model_skin.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x); exit(1); } } while (0)

int main(void)
{
	unsigned int palette[256], pixels[64];
	unsigned char original[64], mask[64];
	unsigned char decoded[TF_MODEL_SKIN_WIDTH * TF_MODEL_SKIN_HEIGHT + 1];
	tf_skin_color_t colors[2];
	int i, team, skin, kind;
	char name[64];
	for (team = 1; team <= 4; ++team) for (kind = 0; kind < 3; ++kind) {
		snprintf(name, sizeof(name), "progs/%s%d.mdl", kind == 2 ? "tfhead" : kind ? "tfheadless" : "tfbody", team);
		CHECK(TF_ModelSkinTeam(name) == team);
		CHECK(TF_ModelSkinKind(name) == kind);
	}
	CHECK(!TF_ModelSkinTeam("progs/player.mdl"));
	CHECK(!TF_ModelSkinTeam("progs/tfbody0.mdl"));
	CHECK(!TF_ModelSkinTeam("progs/tfbody5.mdl"));
	CHECK(!TF_ModelSkinTeam("progs/tfbody1.mdl.extra"));
	CHECK(!TF_ModelSkinTeam("progs/tfbody"));
	CHECK(!TF_ModelSkinTeam("tfbody1.mdl"));
	for (kind = 0; kind < 3; ++kind) for (skin = 0; skin < 10; ++skin) {
		int top = 0, bottom = 0;
		memset(decoded, 255, sizeof(decoded));
		int width = kind == 2 ? 68 : TF_MODEL_SKIN_WIDTH;
		int height = kind == 2 ? 50 : TF_MODEL_SKIN_HEIGHT;
		int count = width * height;
		CHECK(TF_ModelSkinMask(kind, skin, width, height, decoded));
		for (i = 0; i < count; ++i) {
			CHECK(!decoded[i] || (decoded[i] >= 16 && decoded[i] < 32) || (decoded[i] >= 96 && decoded[i] < 112));
			top += decoded[i] >= 16 && decoded[i] < 32;
			bottom += decoded[i] >= 96 && decoded[i] < 112;
		}
		CHECK((kind == 2 || (top && bottom)) && decoded[count] == 255);
	}
	CHECK(!TF_ModelSkinMask(0, 10, 296, 194, decoded));
	CHECK(!TF_ModelSkinMask(0, -1, 296, 194, decoded));
	CHECK(!TF_ModelSkinMask(0, 1, 320, 200, decoded));
	for (i = 0; i < 256; ++i) {
		unsigned char *p = (unsigned char *)&palette[i];
		p[0] = i; p[1] = 255-i; p[2] = i/2; p[3] = 255;
	}
	for (i = 0; i < 16; ++i) {
		unsigned char *p = (unsigned char *)&palette[16+i];
		p[0] = p[1] = p[2] = 16*(i+1)-1;
		p = (unsigned char *)&palette[96+i];
		p[0] = p[1] = p[2] = 255-16*i;
	}
	for (i = 0; i < 64; ++i) {
		original[i] = 192+i; /* Includes fullbrights and ordinary pixels of the same baked color. */
		mask[i] = i < 16 ? 16+i : i < 32 ? 96+i-16 : 0;
	}
	memset(colors, 0, sizeof(colors));
	TF_ModelSkinTranslate(original, mask, 64, palette, colors, pixels);
	for (i = 0; i < 64; ++i) CHECK(pixels[i] == palette[original[i]]);
	colors[0].enabled = colors[1].enabled = 1;
	colors[0].palette = 4; colors[1].palette = 13;
	TF_ModelSkinTranslate(original, mask, 64, palette, colors, pixels);
	for (i = 0; i < 16; ++i) CHECK(pixels[i] == palette[64+i]);
	for (i = 0; i < 16; ++i) CHECK(pixels[16+i] == palette[208+15-i]);
	for (i = 32; i < 64; ++i) CHECK(pixels[i] == palette[original[i]]);
	colors[0].rgb = colors[1].rgb = 1;
	colors[0].color[0] = 255; colors[1].color[2] = 255;
	TF_ModelSkinTranslate(original, mask, 64, palette, colors, pixels);
	for (i = 0; i < 16; ++i) {
		unsigned char *p = (unsigned char *)&pixels[i];
		CHECK(p[0] == 16*(i+1)-1 && !p[1] && !p[2] && p[3] == 255);
		p = (unsigned char *)&pixels[16+i];
		CHECK(!p[0] && !p[1] && p[2] == 255-16*i && p[3] == 255);
	}
	colors[0].enabled = 0;
	colors[1].color[2] = 0; colors[1].color[1] = 255;
	TF_ModelSkinTranslate(original, mask, 64, palette, colors, pixels);
	for (i = 0; i < 16; ++i) CHECK(pixels[i] == palette[original[i]]);
	for (i = 0; i < 16; ++i) CHECK(((unsigned char *)&pixels[16+i])[1] == 255-16*i);
	puts("PASS: twelve identities, 30 bounded masks, palette/RGB shading, selective ramps, settings changes and forcing off");
	return 0;
}
