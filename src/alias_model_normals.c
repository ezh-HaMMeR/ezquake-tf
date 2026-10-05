/* Copyright (C) 2026 ezQuake team. GPL-2.0-or-later. */
#include "alias_model_normals.h"
#include "glsl/constants.glsl"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static uint32_t PositionKey(float value)
{
	uint32_t bits;
	memcpy(&bits, &value, sizeof(bits));
	/* VectorCompare considers +0 and -0 equal. Other IEEE float encodings
	 * may be sorted in any order, provided identical coordinates are adjacent. */
	return (bits & UINT32_C(0x7fffffff)) == 0 ? 0 : bits;
}

static int CompareVertices(const void* lhs, const void* rhs)
{
	const vbo_model_vert_t* a = *(vbo_model_vert_t* const*)lhs;
	const vbo_model_vert_t* b = *(vbo_model_vert_t* const*)rhs;
	int axis;
	for (axis = 0; axis < 3; ++axis) {
		uint32_t x = PositionKey(a->position[axis]), y = PositionKey(b->position[axis]);
		if (x != y) return x < y ? -1 : 1;
	}
	/* All pointers belong to the same pose array. Retain original addition
	 * order, even though qsort itself need not be stable. */
	return a < b ? -1 : a > b;
}

static int SamePosition(const vbo_model_vert_t* a, const vbo_model_vert_t* b)
{
	return a->position[0] == b->position[0] && a->position[1] == b->position[1]
		&& a->position[2] == b->position[2];
}

void AliasModel_FixNormals(vbo_model_vert_t* vertices, int count, vbo_model_vert_t** scratch)
{
	int i, end, first, k, axis;
	if (count < 2) return;
	for (i = 0; i < count; ++i) scratch[i] = vertices + i;
	qsort(scratch, count, sizeof(*scratch), CompareVertices);
	for (i = 0; i < count; i = end) {
		float normal[3], scale;
		for (end = i + 1; end < count && SamePosition(scratch[i], scratch[end]); ++end) {}
		/* Match the old loop for pre-marked vertices too: skip marked leaders,
		 * but include all later matches in the average, marked or otherwise. */
		for (first = i; first < end && (scratch[first]->flags & AM_VERTEX_NORMALFIXED); ++first) {}
		if (end - first < 2) continue;
		memcpy(normal, scratch[first]->normal, sizeof(normal));
		for (k = first + 1; k < end; ++k)
			for (axis = 0; axis < 3; ++axis) normal[axis] += scratch[k]->normal[axis];
		scale = 1.0f / (end - first);
		for (axis = 0; axis < 3; ++axis) normal[axis] *= scale;
		memcpy(scratch[first]->normal, normal, sizeof(normal));
		for (k = first + 1; k < end; ++k) {
			memcpy(scratch[k]->normal, normal, sizeof(normal));
			scratch[k]->flags |= AM_VERTEX_NORMALFIXED;
		}
	}
}
