#include "cfg_editor_model.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *CFGModel_CopyString(const char *text)
{
	size_t length = text ? strlen(text) : 0;
	char *copy = (char *)malloc(length + 1);
	if (!copy) {
		return NULL;
	}
	if (length) {
		memcpy(copy, text, length);
	}
	copy[length] = '\0';
	return copy;
}

static int CFGModel_StringEqual(const char *left, const char *right, int case_sensitive)
{
	if (!left || !right) {
		return left == right;
	}
	if (case_sensitive) {
		return !strcmp(left, right);
	}
	while (*left && *right) {
		if (tolower((unsigned char)*left) != tolower((unsigned char)*right)) {
			return 0;
		}
		left++;
		right++;
	}
	return *left == *right;
}

void CFGModel_Init(cfg_editor_model_t *model)
{
	if (model) {
		memset(model, 0, sizeof(*model));
	}
}

static void CFGModel_FreeFile(cfg_model_file_t *file)
{
	size_t i;
	if (!file) {
		return;
	}
	free(file->id);
	free(file->path);
	free(file->role);
	for (i = 0; i < file->inherits_count; ++i) {
		free(file->inherits[i]);
	}
	free(file->inherits);
	CFGDoc_Free(&file->document);
	memset(file, 0, sizeof(*file));
}

void CFGModel_Free(cfg_editor_model_t *model)
{
	size_t i;
	if (!model) {
		return;
	}
	for (i = 0; i < model->file_count; ++i) {
		CFGModel_FreeFile(&model->files[i]);
	}
	free(model->files);
	memset(model, 0, sizeof(*model));
}

cfg_model_file_t *CFGModel_FindFile(cfg_editor_model_t *model, const char *id)
{
	size_t i;
	if (!model || !id) {
		return NULL;
	}
	for (i = 0; i < model->file_count; ++i) {
		if (!strcmp(model->files[i].id, id)) {
			return &model->files[i];
		}
	}
	return NULL;
}

const cfg_model_file_t *CFGModel_FindFileConst(const cfg_editor_model_t *model, const char *id)
{
	return CFGModel_FindFile((cfg_editor_model_t *)model, id);
}

int CFGModel_AddFileFromMemory(cfg_editor_model_t *model,
	const char *id, const char *path, const char *role,
	const char *const *inherits, size_t inherits_count,
	const unsigned char *data, size_t data_length)
{
	cfg_model_file_t *file;
	size_t i;

	if (!model || !id || !path || !role || CFGModel_FindFile(model, id)) {
		return 0;
	}
	if (model->file_count == model->file_capacity) {
		size_t capacity = model->file_capacity ? model->file_capacity * 2 : 16;
		cfg_model_file_t *files = (cfg_model_file_t *)realloc(model->files, capacity * sizeof(*files));
		if (!files) {
			return 0;
		}
		model->files = files;
		model->file_capacity = capacity;
	}

	file = &model->files[model->file_count];
	memset(file, 0, sizeof(*file));
	file->id = CFGModel_CopyString(id);
	file->path = CFGModel_CopyString(path);
	file->role = CFGModel_CopyString(role);
	if (!file->id || !file->path || !file->role) {
		CFGModel_FreeFile(file);
		return 0;
	}
	if (inherits_count) {
		file->inherits = (char **)calloc(inherits_count, sizeof(*file->inherits));
		if (!file->inherits) {
			CFGModel_FreeFile(file);
			return 0;
		}
		for (i = 0; i < inherits_count; ++i) {
			file->inherits[i] = CFGModel_CopyString(inherits[i]);
			if (!file->inherits[i]) {
				file->inherits_count = i;
				CFGModel_FreeFile(file);
				return 0;
			}
		}
		file->inherits_count = inherits_count;
	}
	if (!CFGDoc_Parse(&file->document, path, data, data_length)) {
		CFGModel_FreeFile(file);
		return 0;
	}
	model->file_count++;
	return 1;
}

static unsigned char *CFGModel_ReadFile(const char *disk_path, size_t *length)
{
	FILE *file;
	long file_length;
	unsigned char *data;

	*length = 0;
	file = fopen(disk_path, "rb");
	if (!file) {
		return NULL;
	}
	if (fseek(file, 0, SEEK_END) || (file_length = ftell(file)) < 0 || fseek(file, 0, SEEK_SET)) {
		fclose(file);
		return NULL;
	}
	data = (unsigned char *)malloc(file_length ? (size_t)file_length : 1);
	if (!data || (file_length && fread(data, 1, (size_t)file_length, file) != (size_t)file_length)) {
		free(data);
		fclose(file);
		return NULL;
	}
	fclose(file);
	*length = (size_t)file_length;
	return data;
}

int CFGModel_AddFileFromDisk(cfg_editor_model_t *model,
	const char *id, const char *path, const char *role,
	const char *const *inherits, size_t inherits_count,
	const char *disk_path)
{
	size_t data_length;
	unsigned char *data = CFGModel_ReadFile(disk_path, &data_length);
	int result;
	if (!data) {
		return 0;
	}
	result = CFGModel_AddFileFromMemory(model, id, path, role, inherits, inherits_count, data, data_length);
	free(data);
	return result;
}

static int CFGModel_NodeMatches(const cfg_node_t *node, cfg_storage_kind_t storage_kind,
	const char *name, const char *on_command, const char *off_command, const char **value)
{
	if (!node || !node->name) {
		return 0;
	}
	switch (storage_kind) {
		case CFG_STORAGE_CVAR:
			if (node->kind == CFG_NODE_COMMAND && CFGModel_StringEqual(node->name, name, 0)) {
				*value = node->value;
				return 1;
			}
			break;
		case CFG_STORAGE_SET:
			if (node->kind == CFG_NODE_SET && CFGModel_StringEqual(node->name, name, 0)) {
				*value = node->value;
				return 1;
			}
			break;
		case CFG_STORAGE_SETINFO:
			if (node->kind == CFG_NODE_SETINFO && CFGModel_StringEqual(node->name, name, 0)) {
				*value = node->value;
				return 1;
			}
			break;
		case CFG_STORAGE_COMMAND:
			if (node->kind == CFG_NODE_COMMAND && CFGModel_StringEqual(node->name, name, 0)) {
				*value = node->value;
				return 1;
			}
			break;
		case CFG_STORAGE_COMMAND_TOGGLE:
			if (node->kind == CFG_NODE_COMMAND && CFGModel_StringEqual(node->name, on_command, 0)) {
				*value = "1";
				return 1;
			}
			if (node->kind == CFG_NODE_COMMAND && CFGModel_StringEqual(node->name, off_command, 0)) {
				*value = "0";
				return 1;
			}
			break;
	}
	return 0;
}

static void CFGModel_FindLocalSetting(cfg_model_file_t *file,
	cfg_storage_kind_t storage_kind, const char *name,
	const char *on_command, const char *off_command,
	cfg_setting_result_t *result)
{
	size_t i;
	for (i = 0; i < file->document.node_count; ++i) {
		const char *value = NULL;
		cfg_node_t *node = &file->document.nodes[i];
		if (CFGModel_NodeMatches(node, storage_kind, name, on_command, off_command, &value)) {
			result->file = file;
			result->node = node;
			result->value = value;
		}
	}
}

static int CFGModel_ResolveRecursive(cfg_editor_model_t *model, cfg_model_file_t *file,
	cfg_storage_kind_t storage_kind, const char *name,
	const char *on_command, const char *off_command,
	cfg_setting_result_t *result, const char **stack, size_t depth)
{
	size_t i;
	if (depth >= 32) {
		return 0;
	}
	for (i = 0; i < depth; ++i) {
		if (!strcmp(stack[i], file->id)) {
			return 0;
		}
	}
	stack[depth++] = file->id;
	for (i = 0; i < file->inherits_count; ++i) {
		cfg_model_file_t *parent = CFGModel_FindFile(model, file->inherits[i]);
		if (!parent || !CFGModel_ResolveRecursive(model, parent, storage_kind, name,
			on_command, off_command, result, stack, depth)) {
			return 0;
		}
	}
	CFGModel_FindLocalSetting(file, storage_kind, name, on_command, off_command, result);
	return 1;
}

int CFGModel_ResolveSetting(cfg_editor_model_t *model, const char *context_file_id,
	cfg_storage_kind_t storage_kind, const char *name,
	const char *on_command, const char *off_command,
	cfg_setting_result_t *result)
{
	cfg_model_file_t *context;
	const char *stack[32];

	if (!model || !context_file_id || !result) {
		return 0;
	}
	context = CFGModel_FindFile(model, context_file_id);
	if (!context) {
		return 0;
	}
	memset(result, 0, sizeof(*result));
	if (!CFGModel_ResolveRecursive(model, context, storage_kind, name,
		on_command, off_command, result, stack, 0)) {
		return 0;
	}
	if (!result->node) {
		return 0;
	}
	result->inherited = result->file != context;
	return 1;
}

size_t CFGModel_MarkSettingManaged(cfg_editor_model_t *model,
	cfg_storage_kind_t storage_kind, const char *name,
	const char *on_command, const char *off_command)
{
	size_t file_index;
	size_t count = 0;
	if (!model) {
		return 0;
	}
	for (file_index = 0; file_index < model->file_count; ++file_index) {
		cfg_model_file_t *file = &model->files[file_index];
		size_t node_index;
		for (node_index = 0; node_index < file->document.node_count; ++node_index) {
			const char *value = NULL;
			cfg_node_t *node = &file->document.nodes[node_index];
			if (CFGModel_NodeMatches(node, storage_kind, name, on_command, off_command, &value)) {
				node->managed = 1;
				count++;
			}
		}
	}
	return count;
}

size_t CFGModel_MarkBindManaged(cfg_editor_model_t *model, const char *command, int case_sensitive)
{
	size_t file_index;
	size_t count = 0;
	if (!model || !command) {
		return 0;
	}
	for (file_index = 0; file_index < model->file_count; ++file_index) {
		cfg_model_file_t *file = &model->files[file_index];
		size_t node_index;
		for (node_index = 0; node_index < file->document.node_count; ++node_index) {
			cfg_node_t *node = &file->document.nodes[node_index];
			if (node->kind == CFG_NODE_BIND && CFGModel_StringEqual(node->value, command, case_sensitive)) {
				node->managed = 1;
				count++;
			}
		}
	}
	return count;
}

size_t CFGModel_CollectMisc(const cfg_editor_model_t *model, const char *file_id,
	int include_comments, cfg_source_ref_t **references)
{
	const cfg_model_file_t *file = CFGModel_FindFileConst(model, file_id);
	cfg_source_ref_t *result;
	size_t count = 0;
	size_t i;

	if (references) {
		*references = NULL;
	}
	if (!file || !references) {
		return 0;
	}
	for (i = 0; i < file->document.node_count; ++i) {
		const cfg_node_t *node = &file->document.nodes[i];
		if (!node->managed && node->kind != CFG_NODE_BLANK &&
			(include_comments || node->kind != CFG_NODE_COMMENT)) {
			count++;
		}
	}
	if (!count) {
		return 0;
	}
	result = (cfg_source_ref_t *)calloc(count, sizeof(*result));
	if (!result) {
		return 0;
	}
	count = 0;
	for (i = 0; i < file->document.node_count; ++i) {
		const cfg_node_t *node = &file->document.nodes[i];
		if (!node->managed && node->kind != CFG_NODE_BLANK &&
			(include_comments || node->kind != CFG_NODE_COMMENT)) {
			result[count].file_id = file->id;
			result[count].path = file->path;
			result[count].node_id = node->id;
			result[count].line_start = node->line_start;
			result[count].line_end = node->line_end;
			result[count].kind = node->kind;
			result[count].raw = node->raw;
			result[count].raw_length = node->raw_length;
			count++;
		}
	}
	*references = result;
	return count;
}

int CFGModel_ReplaceSource(cfg_editor_model_t *model, const cfg_source_ref_t *reference,
	const unsigned char *raw, size_t raw_length)
{
	cfg_model_file_t *file;
	if (!model || !reference) {
		return 0;
	}
	file = CFGModel_FindFile(model, reference->file_id);
	if (!file || !CFGDoc_ReplaceNode(&file->document, reference->node_id, raw, raw_length)) {
		return 0;
	}
	file->dirty = 1;
	return 1;
}

int CFGModel_SerializeFile(const cfg_editor_model_t *model, const char *file_id,
	unsigned char **data, size_t *length)
{
	const cfg_model_file_t *file = CFGModel_FindFileConst(model, file_id);
	return file && CFGDoc_Serialize(&file->document, data, length);
}

void CFGModel_FormatSourceLabel(const cfg_source_ref_t *reference, char *buffer, size_t buffer_size)
{
	if (!buffer || !buffer_size) {
		return;
	}
	if (!reference) {
		buffer[0] = '\0';
		return;
	}
	if (reference->line_start == reference->line_end) {
		snprintf(buffer, buffer_size, "%s:%u [#%u]", reference->path,
			reference->line_start, reference->node_id);
	}
	else {
		snprintf(buffer, buffer_size, "%s:%u-%u [#%u]", reference->path,
			reference->line_start, reference->line_end, reference->node_id);
	}
}
