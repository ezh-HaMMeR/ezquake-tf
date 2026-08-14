#ifndef EZQUAKE_CFG_EDITOR_DICTIONARY_H
#define EZQUAKE_CFG_EDITOR_DICTIONARY_H

#include "cfg_editor_model.h"

typedef enum cfg_widget_type_e {
	CFG_WIDGET_TEXT,
	CFG_WIDGET_PASSWORD,
	CFG_WIDGET_NUMBER,
	CFG_WIDGET_SLIDER,
	CFG_WIDGET_CHECKBOX,
	CFG_WIDGET_SELECT,
	CFG_WIDGET_COLOR,
	CFG_WIDGET_TEXTAREA,
	CFG_WIDGET_ACTION
} cfg_widget_type_t;

typedef struct cfg_dictionary_option_s {
	char *value;
	char *label;
	char *label_en;
} cfg_dictionary_option_t;

typedef struct cfg_setting_definition_s {
	char *id;
	char **scopes;
	size_t scope_count;
	cfg_storage_kind_t storage_kind;
	char *name;
	char *on_command;
	char *off_command;
	cfg_widget_type_t widget_type;
	char *checked_value;
	char *unchecked_value;
	double minimum;
	double maximum;
	double step;
	int has_minimum;
	int has_maximum;
	int has_step;
	int rows;
	int max_length;
	cfg_dictionary_option_t *options;
	size_t option_count;
	char *label;
	char *label_en;
	char *description;
	char *description_en;
	char *group;
	char *apply;
	int order;
	int advanced;
} cfg_setting_definition_t;

typedef struct cfg_bind_definition_s {
	char *id;
	char **scopes;
	size_t scope_count;
	char *command;
	char *label;
	char *label_en;
	char *description;
	char *description_en;
	char *group;
	char *conflict_policy;
	int order;
	int max_keys;
	int case_sensitive;
} cfg_bind_definition_t;

typedef struct cfg_editor_dictionary_s {
	cfg_setting_definition_t *settings;
	size_t setting_count;
	cfg_bind_definition_t *binds;
	size_t bind_count;
} cfg_editor_dictionary_t;

typedef struct cfg_dictionary_apply_result_s {
	size_t setting_nodes;
	size_t bind_nodes;
} cfg_dictionary_apply_result_t;

void CFGDictionary_Init(cfg_editor_dictionary_t *dictionary);
void CFGDictionary_Free(cfg_editor_dictionary_t *dictionary);
int CFGDictionary_Load(cfg_editor_dictionary_t *dictionary,
	const char *settings_path, const char *binds_path,
	char *error, size_t error_size);
int CFGDictionary_ApplyToModel(const cfg_editor_dictionary_t *dictionary,
	cfg_editor_model_t *model, cfg_dictionary_apply_result_t *result,
	char *error, size_t error_size);

#endif
