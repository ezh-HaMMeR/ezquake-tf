/* Compare every output byte with the legacy quadratic algorithm. */
#include "alias_model_normals.h"
#include "glsl/constants.glsl"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void Legacy(vbo_model_vert_t* v, int count)
{
	int j, k, axis, matches;
	for (j = 0; j < count; ++j) {
		float normal[3];
		if (v[j].flags & AM_VERTEX_NORMALFIXED) continue;
		memcpy(normal, v[j].normal, sizeof(normal));
		matches = 1;
		for (k = j + 1; k < count; ++k) {
			if (v[j].position[0] == v[k].position[0] && v[j].position[1] == v[k].position[1] && v[j].position[2] == v[k].position[2]) {
				for (axis = 0; axis < 3; ++axis) normal[axis] += v[k].normal[axis];
				++matches;
			}
		}
		if (matches > 1) {
			float scale = 1.0f / matches;
			for (axis = 0; axis < 3; ++axis) normal[axis] *= scale;
			memcpy(v[j].normal, normal, sizeof(normal));
			for (k = j + 1; k < count; ++k) {
				if (v[j].position[0] == v[k].position[0] && v[j].position[1] == v[k].position[1] && v[j].position[2] == v[k].position[2]) {
					memcpy(v[k].normal, normal, sizeof(normal));
					v[k].flags |= AM_VERTEX_NORMALFIXED;
				}
			}
		}
	}
}

static unsigned int seed = 17;
static unsigned int Next(void) { seed = seed * 1664525u + 1013904223u; return seed; }

static void Check(int count, int mode)
{
	int i, axis;
	vbo_model_vert_t* old = calloc((size_t)count + 2, sizeof(*old));
	vbo_model_vert_t* now = calloc((size_t)count + 2, sizeof(*now));
	vbo_model_vert_t** scratch = malloc(((size_t)count + 1) * sizeof(*scratch));
	if (!old || !now || !scratch) exit(2);
	for (i = 0; i < count + 2; ++i) {
		for (axis = 0; axis < 3; ++axis) {
			old[i].position[axis] = mode == 0 ? (float)i : mode == 1 ? 0 : (float)(Next() % 7) - 3;
			old[i].normal[axis] = ((int)(Next() % 10001) - 5000) / 5000.0f;
			old[i].direction[axis] = (float)Next();
		}
		old[i].texture_coords[0] = i / 100.0f;
		old[i].lightnormalindex = i;
		old[i].flags = mode == 3 ? Next() % 8 : AM_VERTEX_NOLERP;
		if (mode == 4) {
			const float special[] = {0.0f, -0.0f, INFINITY, -INFINITY, NAN, 1.0f, -1.0f};
			old[i].position[0] = special[i % 7];
			old[i].position[1] = old[i].position[2] = 0;
		}
	}
	memcpy(now, old, ((size_t)count + 2) * sizeof(*old));
	/* Sentinel vertices prove that processing a pose cannot touch neighbours. */
	Legacy(old + 1, count);
	AliasModel_FixNormals(now + 1, count, scratch);
	if (memcmp(old, now, ((size_t)count + 2) * sizeof(*old))) {
		fprintf(stderr, "Normal mismatch: count=%d mode=%d\n", count, mode); exit(1);
	}
	/* Repeated preparation must also preserve the legacy flags semantics. */
	Legacy(old + 1, count);
	AliasModel_FixNormals(now + 1, count, scratch);
	if (memcmp(old, now, ((size_t)count + 2) * sizeof(*old))) exit(1);
	free(scratch); free(now); free(old);
}

int main(void)
{
	int mode, n;
	for (mode = 0; mode < 5; ++mode) {
		for (n = 0; n < 100; ++n) Check(n, mode);
		Check(4096, mode);
	}
	puts("505 legacy-equivalence cases passed (unique/coincident/random/flags/IEEE edge cases).");
	return 0;
}
