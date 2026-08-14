/* File-backed, cvar-independent data model for the experimental config editor. */

#ifndef EZQUAKE_CFG_EDITOR_MODEL_H
#define EZQUAKE_CFG_EDITOR_MODEL_H

#include "cfg_editor_parser.h"

typedef enum cfg_storage_kind_e {
	CFG_STORAGE_CVAR,
	CFG_STORAGE_SET,
	CFG_STORAGE_SETINFO,
	CFG_STORAGE_COMMAND,
	CFG_STORAGE_COMMAND_TOGGLE
} cfg_storage_kind_t;

typedef struct cfg_model_file_s {
	char *id;
	char *path;
	char *role;
	char **inherits;
	size_t inherits_count;
	cfg_document_t document;
	int dirty;
} cfg_model_file_t;

typedef struct cfg_editor_model_s {
	cfg_model_file_t *files;
	size_t file_count;
	size_t file_capacity;
} cfg_editor_model_t;

typedef struct cfg_setting_result_s {
	cfg_model_file_t *file;
	cfg_node_t *node;
	const char *value;
	int inherited;
} cfg_setting_result_t;

typedef struct cfg_source_ref_s {
	const char *file_id;
	const char *path;
	unsigned int node_id;
	unsigned int line_start;
	unsigned int line_end;
	cfg_node_kind_t kind;
	const unsigned char *raw;
	size_t raw_length;
} cfg_source_ref_t;

void CFGModel_Init(cfg_editor_model_t *model);
void CFGModel_Free(cfg_editor_model_t *model);
void CFGModel_ClearManaged(cfg_editor_model_t *model);

int CFGModel_LoadManifest(cfg_editor_model_t *model, const char *manifest_path,
	const char *config_root, char *error, size_t error_size);

int CFGModel_AddFileFromMemory(cfg_editor_model_t *model,
	const char *id, const char *path, const char *role,
	const char *const *inherits, size_t inherits_count,
	const unsigned char *data, size_t data_length);
int CFGModel_AddFileFromDisk(cfg_editor_model_t *model,
	const char *id, const char *path, const char *role,
	const char *const *inherits, size_t inherits_count,
	const char *disk_path);

cfg_model_file_t *CFGModel_FindFile(cfg_editor_model_t *model, const char *id);
const cfg_model_file_t *CFGModel_FindFileConst(const cfg_editor_model_t *model, const char *id);

int CFGModel_ResolveSetting(cfg_editor_model_t *model, const char *context_file_id,
	cfg_storage_kind_t storage_kind, const char *name,
	const char *on_command, const char *off_command,
	cfg_setting_result_t *result);

size_t CFGModel_MarkSettingManaged(cfg_editor_model_t *model,
	cfg_storage_kind_t storage_kind, const char *name,
	const char *on_command, const char *off_command);
size_t CFGModel_MarkSettingManagedInFile(cfg_editor_model_t *model, const char *file_id,
	cfg_storage_kind_t storage_kind, const char *name,
	const char *on_command, const char *off_command);
size_t CFGModel_MarkBindManaged(cfg_editor_model_t *model, const char *command, int case_sensitive);
size_t CFGModel_MarkBindManagedInFile(cfg_editor_model_t *model, const char *file_id,
	const char *command, int case_sensitive);

size_t CFGModel_CollectMisc(const cfg_editor_model_t *model, const char *file_id,
	int include_comments, cfg_source_ref_t **references);
int CFGModel_ReplaceSource(cfg_editor_model_t *model, const cfg_source_ref_t *reference,
	const unsigned char *raw, size_t raw_length);
int CFGModel_SerializeFile(const cfg_editor_model_t *model, const char *file_id,
	unsigned char **data, size_t *length);
int CFGModel_ReplaceFileContents(cfg_editor_model_t *model, const char *file_id,
	const unsigned char *data, size_t length);
void CFGModel_FormatSourceLabel(const cfg_source_ref_t *reference, char *buffer, size_t buffer_size);

#endif /* EZQUAKE_CFG_EDITOR_MODEL_H */
