#include "cfg_editor_model.h"

#include <jansson.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CFG_MANIFEST_MAX_INHERITS 16

static void CFGManifest_Error(char *error, size_t error_size, const char *format, ...)
{
	va_list args;
	if (!error || !error_size) return;
	va_start(args, format);
	vsnprintf(error, error_size, format, args);
	va_end(args);
}

static int CFGManifest_JoinPath(char *output, size_t output_size,
	const char *root, const char *relative)
{
	size_t length = strlen(root);
	int result = snprintf(output, output_size, "%s%s%s", root,
		length && root[length - 1] != '/' && root[length - 1] != '\\' ? "/" : "", relative);
	return result >= 0 && (size_t)result < output_size;
}

static int CFGManifest_Visit(const cfg_editor_model_t *model, size_t file_index,
	unsigned char *state, char *error, size_t error_size)
{
	const cfg_model_file_t *file = &model->files[file_index];
	size_t parent_index, candidate;

	if (state[file_index] == 2) return 1;
	if (state[file_index] == 1) {
		CFGManifest_Error(error, error_size, "inheritance cycle at '%s'", file->id);
		return 0;
	}
	state[file_index] = 1;
	for (parent_index = 0; parent_index < file->inherits_count; ++parent_index) {
		for (candidate = 0; candidate < model->file_count; ++candidate) {
			if (!strcmp(model->files[candidate].id, file->inherits[parent_index])) break;
		}
		if (candidate == model->file_count) {
			CFGManifest_Error(error, error_size, "file '%s' inherits unknown file '%s'",
				file->id, file->inherits[parent_index]);
			return 0;
		}
		if (!CFGManifest_Visit(model, candidate, state, error, error_size)) return 0;
	}
	state[file_index] = 2;
	return 1;
}

static int CFGManifest_ValidateParents(const cfg_editor_model_t *model,
	char *error, size_t error_size)
{
	unsigned char *state = (unsigned char *)calloc(model->file_count ? model->file_count : 1, 1);
	size_t file_index;
	if (!state) {
		CFGManifest_Error(error, error_size, "out of memory while validating inheritance");
		return 0;
	}
	for (file_index = 0; file_index < model->file_count; ++file_index) {
		if (!CFGManifest_Visit(model, file_index, state, error, error_size)) {
			free(state);
			return 0;
		}
	}
	free(state);
	return 1;
}

int CFGModel_LoadManifest(cfg_editor_model_t *model, const char *manifest_path,
	const char *game_root, char *error, size_t error_size)
{
	json_error_t json_error;
	json_t *root = NULL, *files, *config_root;
	cfg_editor_model_t loaded;
	size_t index;
	json_t *entry;
	char config_directory[1024];

	if (error && error_size) error[0] = '\0';
	if (!model || !manifest_path || !game_root || !game_root[0]) {
		CFGManifest_Error(error, error_size, "manifest path and installed game root are required");
		return 0;
	}
	root = json_load_file(manifest_path, JSON_REJECT_DUPLICATES, &json_error);
	if (!root) {
		CFGManifest_Error(error, error_size, "%s:%d: %s", manifest_path,
			json_error.line, json_error.text);
		return 0;
	}
	files = json_object_get(root, "files");
	config_root = json_object_get(root, "config_root");
	if (!json_is_integer(json_object_get(root, "schema_version"))
		|| json_integer_value(json_object_get(root, "schema_version")) != 1
		|| !json_is_array(files) || !json_is_string(config_root)
		|| !json_string_length(config_root)
		|| !CFGManifest_JoinPath(config_directory, sizeof(config_directory),
			game_root, json_string_value(config_root))) {
		CFGManifest_Error(error, error_size, "unsupported or malformed managed-files manifest");
		json_decref(root);
		return 0;
	}

	CFGModel_Init(&loaded);
	json_array_foreach(files, index, entry) {
		json_t *id = json_object_get(entry, "id");
		json_t *path = json_object_get(entry, "path");
		json_t *role = json_object_get(entry, "role");
		json_t *inherits = json_object_get(entry, "inherits");
		const char *parents[CFG_MANIFEST_MAX_INHERITS];
		size_t parent_count = 0, parent_index;
		json_t *parent;
		char full_path[1024];

		if (!json_is_object(entry) || !json_is_string(id) || !json_is_string(path)
			|| !json_is_string(role) || !json_is_array(inherits)) {
			CFGManifest_Error(error, error_size, "files[%u] is malformed", (unsigned)index);
			goto fail;
		}
		if (CFGModel_FindFileConst(&loaded, json_string_value(id))) {
			CFGManifest_Error(error, error_size, "duplicate file id '%s'", json_string_value(id));
			goto fail;
		}
		json_array_foreach(inherits, parent_index, parent) {
			if (!json_is_string(parent) || parent_count >= CFG_MANIFEST_MAX_INHERITS) {
				CFGManifest_Error(error, error_size, "invalid inheritance list for '%s'", json_string_value(id));
				goto fail;
			}
			parents[parent_count++] = json_string_value(parent);
		}
		if (!CFGManifest_JoinPath(full_path, sizeof(full_path), config_directory, json_string_value(path))
			|| !CFGModel_AddFileFromDisk(&loaded, json_string_value(id), json_string_value(path),
				json_string_value(role), parents, parent_count, full_path)) {
			CFGManifest_Error(error, error_size, "cannot load managed file '%s'", json_string_value(path));
			goto fail;
		}
	}

	if (!CFGManifest_ValidateParents(&loaded, error, error_size)) goto fail;
	CFGModel_Free(model);
	*model = loaded;
	json_decref(root);
	return 1;

fail:
	CFGModel_Free(&loaded);
	json_decref(root);
	return 0;
}
