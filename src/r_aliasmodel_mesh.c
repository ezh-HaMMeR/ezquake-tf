/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// gl_mesh.c: triangle model functions

#include "quakedef.h"
#include "gl_model.h"
#include "r_local.h"
#include "r_aliasmodel.h"
#include "glsl/constants.glsl"

#ifdef RENDERER_OPTION_MODERN_OPENGL
void GLM_CreateAliasModelVAO(void);
#endif

// Also used by .md3
void GL_AliasModelSetVertexDirection(int num_triangles, vbo_model_vert_t* vbo_buffer, int v1, int v2, qbool limit_lerp, int key_pose)
{
	int j, k;

	for (j = 0; j < num_triangles; ++j) {
		for (k = 0; k < 3; ++k, ++v1, ++v2) {
			VectorSubtract(vbo_buffer[v2].position, vbo_buffer[v1].position, vbo_buffer[v1].direction);

			vbo_buffer[v1].flags = 0;
			if (limit_lerp) {
				if ((vbo_buffer[v1].position[0] > 0) != (vbo_buffer[v2].position[0] > 0)) {
					vbo_buffer[v1].flags |= AM_VERTEX_NOLERP;
				}
			}
		}
	}
}

// Also used by .md3
void GL_AliasModelFixNormals(vbo_model_vert_t* vbo_buffer, int v, int vertsPerPose)
{
	vbo_model_vert_t** scratch;
	if (vertsPerPose < 2) return;
	/* Sort a temporary index, not the VBO: triangle order, UVs and animation
	 * offsets stay intact. Avoid quadratic scans on high-detail MDL/MD3s. */
	scratch = Q_malloc((size_t)vertsPerPose * sizeof(*scratch));
	AliasModel_FixNormals(vbo_buffer + v, vertsPerPose, scratch);
	Q_free(scratch);
}

void GL_PrepareAliasModel(model_t* m, aliashdr_t* hdr)
{
	// 
	extern float r_avertexnormals[NUMVERTEXNORMALS][3];
	vbo_model_vert_t* vbo_buffer;
	int pose, j, k, v;
	int f;

	v = 0;
	hdr->poseverts = hdr->vertsPerPose = 3 * hdr->numtris;
	m->vertsInVBO = hdr->vertsPerPose * hdr->numposes;
	m->temp_vbo_buffer = vbo_buffer = Q_malloc_named(m->vertsInVBO * sizeof(vbo_model_vert_t), m->name);

	// 
	for (pose = 0; pose < hdr->numposes; ++pose) {
		v = pose * hdr->vertsPerPose;
		for (j = 0; j < hdr->numtris; ++j) {
			for (k = 0; k < 3; ++k, ++v) {
				trivertx_t* src;
				float x, y, z, s, t;
				int l;
				int vert = triangles[j].vertindex[k];

				src = &poseverts[pose][vert];

				l = src->lightnormalindex;
				x = src->v[0] * hdr->scale[0] + hdr->scale_origin[0];
				y = src->v[1] * hdr->scale[1] + hdr->scale_origin[1];
				z = src->v[2] * hdr->scale[2] + hdr->scale_origin[2];
				if (m->modhint == MOD_EYES) {
					x *= 2;
					y *= 2;
					z *= 2;
					z -= 30;
				}
				s = stverts[vert].s;
				t = stverts[vert].t;

				if (!triangles[j].facesfront && stverts[vert].onseam) {
					s += pheader->skinwidth / 2;	// on back side
				}
				s = (s + 0.5) / pheader->skinwidth;
				t = (t + 0.5) / pheader->skinheight;

				memset(&vbo_buffer[v], 0, sizeof(vbo_buffer[v]));
				VectorSet(vbo_buffer[v].position, x, y, z);
				vbo_buffer[v].texture_coords[0] = s;
				vbo_buffer[v].texture_coords[1] = t;
				VectorCopy(r_avertexnormals[l], vbo_buffer[v].normal);
				vbo_buffer[v].flags = 0;
				vbo_buffer[v].lightnormalindex = l;
			}
		}

		GL_AliasModelFixNormals(vbo_buffer, pose * hdr->vertsPerPose, hdr->vertsPerPose);
	}

	// Go back through and set directions
	for (f = 0; f < hdr->numframes; ++f) {
		maliasframedesc_t* frame = &hdr->frames[f];

		if (frame->numposes > 1) {
			// This frame has animated poses, so link them all together
			for (pose = 0; pose < frame->numposes - 1; ++pose) {
				GL_AliasModelSetVertexDirection(hdr->numtris, vbo_buffer, hdr->vertsPerPose * (frame->firstpose + pose), hdr->vertsPerPose * (frame->firstpose + pose + 1), m->renderfx & RF_LIMITLERP, 0);
			}
		}
		else {
			// Find next frame's pose
			maliasframedesc_t* frame2 = R_AliasModelFindFrame(hdr, frame->groupname, frame->groupnumber + 1);
			if (!frame2) {
				frame2 = R_AliasModelFindFrame(hdr, frame->groupname, 1 + Mod_ExpectedNextFrame(m, frame->groupnumber, hdr->numframes));
			}

			if (frame2) {
				GL_AliasModelSetVertexDirection(hdr->numtris, vbo_buffer, hdr->vertsPerPose * frame->firstpose, hdr->vertsPerPose * frame2->firstpose, m->renderfx & RF_LIMITLERP, 0);
				frame->nextpose = frame2->firstpose;
			}
		}
	}
}

#ifdef RENDERER_OPTION_CLASSIC_OPENGL
extern cvar_t gl_program_aliasmodels;
#define R_GLSLAliasModelRendering() ((!R_UseImmediateOpenGL()) || gl_program_aliasmodels.integer)
#else
#define R_GLSLAliasModelRendering() (1)
#endif

/* The GPU buffer is reusable across maps as long as every requested model
 * still occupies its recorded slot. CPU buffers are a bounded rebuild cache. */
extern model_t mod_known[];
extern int mod_numknown;
cvar_t r_modelcache = { "r_modelcache", "1" };
cvar_t r_modelcache_mb = { "r_modelcache_mb", "256" };
cvar_t r_modelcache_stats = { "r_modelcache_stats", "0" };
static qbool alias_vbo_cached;
static qbool alias_vbo_shader_mode;
static unsigned alias_vbo_generation = 1;
static size_t alias_cpu_kept;

void R_InvalidateAliasModelCache(void)
{
    alias_vbo_cached = false;
    ++alias_vbo_generation;
    if (!alias_vbo_generation) ++alias_vbo_generation;
}

static qbool R_ModelSlotResident(model_t* mod)
{
    return !mod || (mod->type != mod_alias && mod->type != mod_alias3)
        || (!mod->needload && mod->cached_data && mod->alias_vbo_generation == alias_vbo_generation);
}

qbool R_AliasModelCacheReusable(void)
{
    int i;
    if (!r_modelcache.integer || !alias_vbo_cached || !buffers.supported
        || !buffers.IsValid(r_buffer_aliasmodel_vertex_data)
        || alias_vbo_shader_mode != !!R_GLSLAliasModelRendering()) return false;
    for (i = 1; i < MAX_MODELS; ++i) if (!R_ModelSlotResident(cl.model_precache[i])) return false;
    for (i = 0; i < MAX_VWEP_MODELS; ++i) if (!R_ModelSlotResident(cl.vw_model_precache[i])) return false;
    for (i = 0; i < custom_model_count; ++i) if (!R_ModelSlotResident(cl_custommodels[i])) return false;
    return true;
}

static void R_TrimAliasCPUCache(void)
{
    size_t kept = 0, budget = (size_t)bound(0, r_modelcache_mb.integer, 2048) * 1024 * 1024;
    int i;
    if (!R_GLSLAliasModelRendering()) return; /* Immediate mode draws from CPU memory. */
    for (i = 0; i < mod_numknown; ++i) {
        model_t* mod = &mod_known[i];
        size_t bytes = (size_t)mod->vertsInVBO * sizeof(vbo_model_vert_t);
        if (!mod->temp_vbo_buffer) continue;
        if (!r_modelcache.integer || mod->alias_vbo_generation != alias_vbo_generation || bytes > budget - kept) {
            Q_free(mod->temp_vbo_buffer);
        }
        else kept += bytes;
    }
    alias_cpu_kept = kept;
}

void R_AliasModelPopulateVBO(model_t* mod, vbo_model_vert_t* aliasModelBuffer, int position)
{
	// Don't delete if using immediate mode as we loop over them ourselves
	if (mod->temp_vbo_buffer && R_GLSLAliasModelRendering()) {
		memcpy(aliasModelBuffer + position, mod->temp_vbo_buffer, mod->vertsInVBO * sizeof(vbo_model_vert_t));

		mod->vbo_start = position;
		mod->alias_vbo_generation = alias_vbo_generation;
	}
}

static void R_ImportSpriteCoordsToVBO(vbo_model_vert_t* verts, int* position)
{
	verts = &verts[*position];

	memset(verts, 0, sizeof(vbo_model_vert_t) * 4);

	VectorSet(verts[0].position, 0, -1, -1);
	verts[0].texture_coords[0] = 1;
	verts[0].texture_coords[1] = 1;
	verts[0].flags = 0;

	VectorSet(verts[1].position, 0, -1, 1);
	verts[1].texture_coords[0] = 1;
	verts[1].texture_coords[1] = 0;
	verts[1].flags = 1;

	VectorSet(verts[2].position, 0, 1, 1);
	verts[2].texture_coords[0] = 0;
	verts[2].texture_coords[1] = 0;
	verts[2].flags = 2;

	VectorSet(verts[3].position, 0, 1, -1);
	verts[3].texture_coords[0] = 0;
	verts[3].texture_coords[1] = 1;
	verts[3].flags = 3;

	*position += 4;
}

static void R_ImportModelToVBO(model_t* mod, vbo_model_vert_t* aliasmodel_data, int* new_vbo_position)
{
	if (mod->type == mod_alias || mod->type == mod_alias3) {
		R_AliasModelPopulateVBO(mod, aliasmodel_data, *new_vbo_position);
		*new_vbo_position += mod->vertsInVBO;
	}
	else if (mod->type == mod_sprite) {
		mod->vbo_start = 0;
	}
	else if (mod->type == mod_brush) {
		mod->vbo_start = 0;
	}
}

void R_CreateAliasModelVBO(void)
{
	double started = Sys_DoubleTime();
	vbo_model_vert_t* aliasModelData;
	int new_vbo_position = 0;
	int required_vbo_length = 4;
	int i;

    if (R_AliasModelCacheReusable()) {
#ifdef RENDERER_OPTION_MODERN_OPENGL
        if (R_UseModernOpenGL()) GLM_CreateAliasModelVAO();
#endif
        R_TrimAliasCPUCache();
        if (r_modelcache_stats.integer) Com_Printf("ModelCache GPU hit: %.3f ms, CPU %.1f MiB\n", (Sys_DoubleTime()-started)*1000, alias_cpu_kept/1048576.0);
        return;
    }
    R_InvalidateAliasModelCache();

	for (i = 1; i < MAX_MODELS; ++i) {
		model_t* mod = cl.model_precache[i];

		if (mod && (mod->type == mod_alias || mod->type == mod_alias3)) {
			required_vbo_length += mod->vertsInVBO;
		}
	}

	for (i = 0; i < MAX_VWEP_MODELS; i++) {
		model_t* mod = cl.vw_model_precache[i];

		if (mod && (mod->type == mod_alias || mod->type == mod_alias3)) {
			required_vbo_length += mod->vertsInVBO;
		}
	}

	// custom models are explicitly loaded by client, not notified by server
	for (i = 0; i < custom_model_count; ++i) {
		model_t* mod = Mod_CustomModel(i, false);

		if (mod && (mod->type == mod_alias || mod->type == mod_alias3)) {
			required_vbo_length += mod->vertsInVBO;
		}
	}

	// Go back through all models, populating VBO content
	aliasModelData = Q_malloc(required_vbo_length * sizeof(vbo_model_vert_t));

	// VBO starts with simple-model/sprite vertices
	R_ImportSpriteCoordsToVBO(aliasModelData, &new_vbo_position);

	for (i = 1; i < MAX_MODELS; ++i) {
		model_t* mod = cl.model_precache[i];

		if (mod) {
			R_ImportModelToVBO(mod, aliasModelData, &new_vbo_position);
		}
	}

	for (i = 0; i < MAX_VWEP_MODELS; i++) {
		model_t* mod = cl.vw_model_precache[i];

		if (mod) {
			R_ImportModelToVBO(mod, aliasModelData, &new_vbo_position);
		}
	}

	for (i = 0; i < custom_model_count; ++i) {
		model_t* mod = Mod_CustomModel(i, false);

		if (mod) {
			R_ImportModelToVBO(mod, aliasModelData, &new_vbo_position);
		}
	}

	buffers.Create(r_buffer_aliasmodel_vertex_data, buffertype_vertex, "aliasmodel-vertex-data", required_vbo_length * sizeof(vbo_model_vert_t), aliasModelData, bufferusage_reuse_many_frames);
#ifdef RENDERER_OPTION_MODERN_OPENGL
	if (R_UseModernOpenGL()) {
		GLM_CreateAliasModelVAO();
#ifdef EZQ_GL_BINDINGPOINT_ALIASMODEL_SSBO
		aliasModel_ssbo = buffers.Create(buffertype_storage, "aliasmodel-vertex-ssbo", required_vbo_length * sizeof(vbo_model_vert_t), aliasModelData, bufferusage_constant_data);
		buffers.BindBase(aliasModel_ssbo, EZQ_GL_BINDINGPOINT_ALIASMODEL_SSBO);
#endif
	}
#endif
	Q_free(aliasModelData);
    alias_vbo_shader_mode = !!R_GLSLAliasModelRendering();
    alias_vbo_cached = alias_vbo_shader_mode;
    R_TrimAliasCPUCache();
    if (r_modelcache_stats.integer) Com_Printf("ModelCache GPU build: %.3f ms, GPU %.1f MiB, CPU %.1f MiB\n", (Sys_DoubleTime()-started)*1000, (double)required_vbo_length*sizeof(vbo_model_vert_t)/1048576.0, alias_cpu_kept/1048576.0);


#ifdef RENDERER_OPTION_CLASSIC_OPENGL
	if (R_UseImmediateOpenGL()) {
		GLC_AllocateAliasPoseBuffer();
	}
#endif
}
