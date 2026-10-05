/* Shared CPU vertex layout and normal preparation; no renderer dependencies. */
#ifndef EZ_ALIAS_MODEL_NORMALS_H
#define EZ_ALIAS_MODEL_NORMALS_H

typedef struct vbo_model_vert_s {
	float position[3];
	int lightnormalindex;
	float normal[3];
	int padding2;
	float direction[3];
	int padding3;
	float texture_coords[2];
	unsigned int flags;
	int padding4;
} vbo_model_vert_t;

/* scratch holds count pointers, supplied by the caller's allocator. */
void AliasModel_FixNormals(vbo_model_vert_t* vertices, int count, vbo_model_vert_t** scratch);

#endif
