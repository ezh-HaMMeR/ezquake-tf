#include "cfg_editor_dictionary.h"

#include <jansson.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CFG_DICTIONARY_MAX_KEYS 4

static void CFGDictionary_Error(char *error, size_t error_size, const char *format, ...)
{
	va_list args;
	if (!error || !error_size) return;
	va_start(args, format);
	vsnprintf(error, error_size, format, args);
	va_end(args);
}

static char *CFGDictionary_CopyString(const char *text)
{
	size_t length = text ? strlen(text) : 0;
	char *copy = (char *)malloc(length + 1);
	if (!copy) return NULL;
	if (length) memcpy(copy, text, length);
	copy[length] = '\0';
	return copy;
}

static char *CFGDictionary_CopyJsonString(json_t *object, const char *name, int required)
{
	json_t *value = json_object_get(object, name);
	if (!value) return required ? NULL : CFGDictionary_CopyString("");
	return json_is_string(value) ? CFGDictionary_CopyString(json_string_value(value)) : NULL;
}

static int CFGDictionary_CopyScopes(json_t *object, char ***scopes, size_t *scope_count)
{
	json_t *array = json_object_get(object, "scope");
	json_t *value;
	size_t index;
	char **result;

	*scopes = NULL;
	*scope_count = 0;
	if (!json_is_array(array) || !json_array_size(array)) return 0;
	result = (char **)calloc(json_array_size(array), sizeof(*result));
	if (!result) return 0;
	json_array_foreach(array, index, value) {
		if (!json_is_string(value) || !(result[index] = CFGDictionary_CopyString(json_string_value(value)))) {
			size_t cleanup;
			for (cleanup = 0; cleanup < index; ++cleanup) free(result[cleanup]);
			free(result);
			return 0;
		}
	}
	*scopes = result;
	*scope_count = json_array_size(array);
	return 1;
}

static int CFGDictionary_StorageKind(const char *type, cfg_storage_kind_t *kind)
{
	if (!strcmp(type, "cvar")) *kind = CFG_STORAGE_CVAR;
	else if (!strcmp(type, "set")) *kind = CFG_STORAGE_SET;
	else if (!strcmp(type, "setinfo")) *kind = CFG_STORAGE_SETINFO;
	else if (!strcmp(type, "command")) *kind = CFG_STORAGE_COMMAND;
	else if (!strcmp(type, "command_toggle")) *kind = CFG_STORAGE_COMMAND_TOGGLE;
	else return 0;
	return 1;
}

static int CFGDictionary_WidgetType(const char *type, cfg_widget_type_t *widget)
{
	if (!strcmp(type, "text")) *widget = CFG_WIDGET_TEXT;
	else if (!strcmp(type, "password")) *widget = CFG_WIDGET_PASSWORD;
	else if (!strcmp(type, "number")) *widget = CFG_WIDGET_NUMBER;
	else if (!strcmp(type, "slider")) *widget = CFG_WIDGET_SLIDER;
	else if (!strcmp(type, "checkbox")) *widget = CFG_WIDGET_CHECKBOX;
	else if (!strcmp(type, "select")) *widget = CFG_WIDGET_SELECT;
	else if (!strcmp(type, "color")) *widget = CFG_WIDGET_COLOR;
	else if (!strcmp(type, "textarea")) *widget = CFG_WIDGET_TEXTAREA;
	else if (!strcmp(type, "action")) *widget = CFG_WIDGET_ACTION;
	else return 0;
	return 1;
}

static void CFGDictionary_FreeSetting(cfg_setting_definition_t *setting)
{
	size_t index;
	free(setting->id);
	for (index = 0; index < setting->scope_count; ++index) free(setting->scopes[index]);
	free(setting->scopes);
	free(setting->name);
	free(setting->on_command);
	free(setting->off_command);
	free(setting->checked_value);
	free(setting->unchecked_value);
	for (index = 0; index < setting->option_count; ++index) {
		free(setting->options[index].value);
		free(setting->options[index].label);
		free(setting->options[index].label_en);
	}
	free(setting->options);
	free(setting->label);
	free(setting->label_en);
	free(setting->description);
	free(setting->description_en);
	free(setting->group);
	free(setting->apply);
	memset(setting, 0, sizeof(*setting));
}

static void CFGDictionary_FreeBind(cfg_bind_definition_t *bind)
{
	size_t index;
	free(bind->id);
	for (index = 0; index < bind->scope_count; ++index) free(bind->scopes[index]);
	free(bind->scopes);
	free(bind->command);
	free(bind->label);
	free(bind->label_en);
	free(bind->description);
	free(bind->description_en);
	free(bind->group);
	free(bind->conflict_policy);
	memset(bind, 0, sizeof(*bind));
}

void CFGDictionary_Init(cfg_editor_dictionary_t *dictionary)
{
	if (dictionary) memset(dictionary, 0, sizeof(*dictionary));
}

void CFGDictionary_Free(cfg_editor_dictionary_t *dictionary)
{
	size_t index;
	if (!dictionary) return;
	for (index = 0; index < dictionary->setting_count; ++index) {
		CFGDictionary_FreeSetting(&dictionary->settings[index]);
	}
	for (index = 0; index < dictionary->bind_count; ++index) {
		CFGDictionary_FreeBind(&dictionary->binds[index]);
	}
	free(dictionary->settings);
	free(dictionary->binds);
	memset(dictionary, 0, sizeof(*dictionary));
}

static int CFGDictionary_ParseOptions(json_t *widget, cfg_setting_definition_t *setting)
{
	json_t *options = json_object_get(widget, "options");
	json_t *option;
	size_t index;
	if (!options) return 1;
	if (!json_is_array(options) || !json_array_size(options)) return 0;
	setting->options = (cfg_dictionary_option_t *)calloc(json_array_size(options), sizeof(*setting->options));
	if (!setting->options) return 0;
	setting->option_count = json_array_size(options);
	json_array_foreach(options, index, option) {
		if (!json_is_object(option)
			|| !(setting->options[index].value = CFGDictionary_CopyJsonString(option, "value", 1))
			|| !(setting->options[index].label = CFGDictionary_CopyJsonString(option, "label", 1))
			|| !(setting->options[index].label_en = CFGDictionary_CopyJsonString(option, "label_en", 1))) {
			return 0;
		}
	}
	return 1;
}

static int CFGDictionary_ParseSetting(json_t *entry, cfg_setting_definition_t *setting)
{
	json_t *storage = json_object_get(entry, "storage");
	json_t *widget = json_object_get(entry, "widget");
	json_t *value;
	const char *type;

	if (!json_is_object(entry) || !json_is_object(storage) || !json_is_object(widget)) return 0;
	setting->id = CFGDictionary_CopyJsonString(entry, "id", 1);
	setting->label = CFGDictionary_CopyJsonString(entry, "label", 1);
	setting->label_en = CFGDictionary_CopyJsonString(entry, "label_en", 1);
	setting->description = CFGDictionary_CopyJsonString(entry, "description", 0);
	setting->description_en = CFGDictionary_CopyJsonString(entry, "description_en", 0);
	setting->group = CFGDictionary_CopyJsonString(entry, "group", 0);
	setting->apply = CFGDictionary_CopyJsonString(entry, "apply", 0);
	if (!setting->id || !setting->label || !setting->label_en || !setting->description || !setting->description_en || !setting->group || !setting->apply
		|| !CFGDictionary_CopyScopes(entry, &setting->scopes, &setting->scope_count)) return 0;

	value = json_object_get(storage, "type");
	if (!json_is_string(value) || !CFGDictionary_StorageKind(json_string_value(value), &setting->storage_kind)) return 0;
	setting->name = CFGDictionary_CopyJsonString(storage, "name", 0);
	setting->on_command = CFGDictionary_CopyJsonString(storage, "on_command", 0);
	setting->off_command = CFGDictionary_CopyJsonString(storage, "off_command", 0);
	if (!setting->name || !setting->on_command || !setting->off_command) return 0;
	if (setting->storage_kind == CFG_STORAGE_COMMAND_TOGGLE) {
		if (!setting->on_command[0] || !setting->off_command[0]) return 0;
	}
	else if (!setting->name[0]) return 0;

	value = json_object_get(widget, "type");
	if (!json_is_string(value)) return 0;
	type = json_string_value(value);
	if (!CFGDictionary_WidgetType(type, &setting->widget_type)) return 0;
	setting->checked_value = CFGDictionary_CopyJsonString(widget, "checked_value", 0);
	setting->unchecked_value = CFGDictionary_CopyJsonString(widget, "unchecked_value", 0);
	if (!setting->checked_value || !setting->unchecked_value || !CFGDictionary_ParseOptions(widget, setting)) return 0;

	value = json_object_get(widget, "min");
	if (value) { if (!json_is_number(value)) return 0; setting->minimum = json_number_value(value); setting->has_minimum = 1; }
	value = json_object_get(widget, "max");
	if (value) { if (!json_is_number(value)) return 0; setting->maximum = json_number_value(value); setting->has_maximum = 1; }
	value = json_object_get(widget, "step");
	if (value) { if (!json_is_number(value)) return 0; setting->step = json_number_value(value); setting->has_step = 1; }
	value = json_object_get(widget, "rows");
	if (value) { if (!json_is_integer(value)) return 0; setting->rows = (int)json_integer_value(value); }
	value = json_object_get(widget, "max_length");
	if (value) { if (!json_is_integer(value)) return 0; setting->max_length = (int)json_integer_value(value); }
	value = json_object_get(entry, "order");
	if (value) { if (!json_is_integer(value)) return 0; setting->order = (int)json_integer_value(value); }
	value = json_object_get(entry, "advanced");
	if (value) { if (!json_is_boolean(value)) return 0; setting->advanced = json_is_true(value); }
	return 1;
}

static int CFGDictionary_ParseBind(json_t *entry, cfg_bind_definition_t *bind)
{
	json_t *value;
	if (!json_is_object(entry)) return 0;
	bind->id = CFGDictionary_CopyJsonString(entry, "id", 1);
	bind->command = CFGDictionary_CopyJsonString(entry, "command", 1);
	bind->label = CFGDictionary_CopyJsonString(entry, "label", 1);
	bind->label_en = CFGDictionary_CopyJsonString(entry, "label_en", 1);
	bind->description = CFGDictionary_CopyJsonString(entry, "description", 0);
	bind->description_en = CFGDictionary_CopyJsonString(entry, "description_en", 0);
	bind->group = CFGDictionary_CopyJsonString(entry, "group", 0);
	bind->conflict_policy = CFGDictionary_CopyJsonString(entry, "conflict_policy", 0);
	bind->max_keys = 2;
	if (!bind->id || !bind->command || !bind->label || !bind->label_en || !bind->description || !bind->description_en || !bind->group
		|| !bind->conflict_policy || !CFGDictionary_CopyScopes(entry, &bind->scopes, &bind->scope_count)) return 0;
	value = json_object_get(entry, "order");
	if (value) { if (!json_is_integer(value)) return 0; bind->order = (int)json_integer_value(value); }
	value = json_object_get(entry, "max_keys");
	if (value) { if (!json_is_integer(value)) return 0; bind->max_keys = (int)json_integer_value(value); }
	value = json_object_get(entry, "case_sensitive");
	if (value) { if (!json_is_boolean(value)) return 0; bind->case_sensitive = json_is_true(value); }
	return bind->max_keys >= 1 && bind->max_keys <= CFG_DICTIONARY_MAX_KEYS;
}

static int CFGDictionary_CheckDuplicateIds(const cfg_editor_dictionary_t *dictionary,
	char *error, size_t error_size)
{
	size_t left, right;
	for (left = 0; left < dictionary->setting_count; ++left) {
		for (right = left + 1; right < dictionary->setting_count; ++right) {
			if (!strcmp(dictionary->settings[left].id, dictionary->settings[right].id)) {
				CFGDictionary_Error(error, error_size, "duplicate setting id '%s'", dictionary->settings[left].id);
				return 0;
			}
		}
	}
	for (left = 0; left < dictionary->bind_count; ++left) {
		for (right = left + 1; right < dictionary->bind_count; ++right) {
			if (!strcmp(dictionary->binds[left].id, dictionary->binds[right].id)) {
				CFGDictionary_Error(error, error_size, "duplicate bind id '%s'", dictionary->binds[left].id);
				return 0;
			}
		}
	}
	return 1;
}

static json_t *CFGDictionary_LoadRoot(const char *path, const char *array_name,
	char *error, size_t error_size)
{
	json_error_t json_error;
	json_t *root = json_load_file(path, JSON_REJECT_DUPLICATES, &json_error);
	if (!root) {
		CFGDictionary_Error(error, error_size, "%s:%d: %s", path, json_error.line, json_error.text);
		return NULL;
	}
	if (!json_is_object(root) || !json_is_integer(json_object_get(root, "schema_version"))
		|| json_integer_value(json_object_get(root, "schema_version")) != 1
		|| !json_is_array(json_object_get(root, array_name))) {
		CFGDictionary_Error(error, error_size, "%s: invalid schema version or '%s' array", path, array_name);
		json_decref(root);
		return NULL;
	}
	return root;
}

int CFGDictionary_Load(cfg_editor_dictionary_t *dictionary,
	const char *settings_path, const char *binds_path,
	char *error, size_t error_size)
{
	json_t *settings_root = NULL, *binds_root = NULL, *array, *entry;
	cfg_editor_dictionary_t loaded;
	size_t index;

	if (error && error_size) error[0] = '\0';
	CFGDictionary_Init(&loaded);
	settings_root = CFGDictionary_LoadRoot(settings_path, "settings", error, error_size);
	if (!settings_root) goto fail;
	binds_root = CFGDictionary_LoadRoot(binds_path, "binds", error, error_size);
	if (!binds_root) goto fail;

	array = json_object_get(settings_root, "settings");
	loaded.setting_count = json_array_size(array);
	loaded.settings = (cfg_setting_definition_t *)calloc(loaded.setting_count ? loaded.setting_count : 1,
		sizeof(*loaded.settings));
	if (!loaded.settings) goto memory_fail;
	json_array_foreach(array, index, entry) {
		if (!CFGDictionary_ParseSetting(entry, &loaded.settings[index])) {
			CFGDictionary_Error(error, error_size, "%s: invalid settings[%u]", settings_path, (unsigned)index);
			goto fail;
		}
	}

	array = json_object_get(binds_root, "binds");
	loaded.bind_count = json_array_size(array);
	loaded.binds = (cfg_bind_definition_t *)calloc(loaded.bind_count ? loaded.bind_count : 1,
		sizeof(*loaded.binds));
	if (!loaded.binds) goto memory_fail;
	json_array_foreach(array, index, entry) {
		if (!CFGDictionary_ParseBind(entry, &loaded.binds[index])) {
			CFGDictionary_Error(error, error_size, "%s: invalid binds[%u]", binds_path, (unsigned)index);
			goto fail;
		}
	}
	if (!CFGDictionary_CheckDuplicateIds(&loaded, error, error_size)) goto fail;

	CFGDictionary_Free(dictionary);
	*dictionary = loaded;
	json_decref(settings_root);
	json_decref(binds_root);
	return 1;

memory_fail:
	CFGDictionary_Error(error, error_size, "out of memory while loading dictionaries");
fail:
	json_decref(settings_root);
	json_decref(binds_root);
	CFGDictionary_Free(&loaded);
	return 0;
}

static int CFGDictionary_ScopeFileId(const char *scope, int bind_scope,
	char *file_id, size_t file_id_size)
{
	int result;
	if (bind_scope && !strcmp(scope, "global")) {
		result = snprintf(file_id, file_id_size, "binds");
	}
	else if (!bind_scope && (!strcmp(scope, "main") || !strcmp(scope, "hud"))) {
		result = snprintf(file_id, file_id_size, "%s", scope);
	}
	else if (!strncmp(scope, "class:", 6) && scope[6]) {
		result = snprintf(file_id, file_id_size, "class_%s", scope + 6);
	}
	else return 0;
	return result >= 0 && (size_t)result < file_id_size;
}

int CFGDictionary_ApplyToModel(const cfg_editor_dictionary_t *dictionary,
	cfg_editor_model_t *model, cfg_dictionary_apply_result_t *result,
	char *error, size_t error_size)
{
	size_t entry_index, scope_index;
	cfg_dictionary_apply_result_t applied = { 0 };
	char file_id[64];

	if (error && error_size) error[0] = '\0';
	if (!dictionary || !model) return 0;
	for (entry_index = 0; entry_index < dictionary->setting_count; ++entry_index) {
		const cfg_setting_definition_t *setting = &dictionary->settings[entry_index];
		for (scope_index = 0; scope_index < setting->scope_count; ++scope_index) {
			if (!CFGDictionary_ScopeFileId(setting->scopes[scope_index], 0, file_id, sizeof(file_id))
				|| !CFGModel_FindFileConst(model, file_id)) {
				CFGDictionary_Error(error, error_size, "setting '%s' has unknown scope '%s'",
					setting->id, setting->scopes[scope_index]);
				return 0;
			}
		}
	}
	for (entry_index = 0; entry_index < dictionary->bind_count; ++entry_index) {
		const cfg_bind_definition_t *bind = &dictionary->binds[entry_index];
		for (scope_index = 0; scope_index < bind->scope_count; ++scope_index) {
			if (!CFGDictionary_ScopeFileId(bind->scopes[scope_index], 1, file_id, sizeof(file_id))
				|| !CFGModel_FindFileConst(model, file_id)) {
				CFGDictionary_Error(error, error_size, "bind '%s' has unknown scope '%s'",
					bind->id, bind->scopes[scope_index]);
				return 0;
			}
		}
	}
	CFGModel_ClearManaged(model);
	for (entry_index = 0; entry_index < dictionary->setting_count; ++entry_index) {
		const cfg_setting_definition_t *setting = &dictionary->settings[entry_index];
		for (scope_index = 0; scope_index < setting->scope_count; ++scope_index) {
			CFGDictionary_ScopeFileId(setting->scopes[scope_index], 0, file_id, sizeof(file_id));
			applied.setting_nodes += CFGModel_MarkSettingManagedInFile(model, file_id,
				setting->storage_kind, setting->name, setting->on_command, setting->off_command);
		}
	}
	for (entry_index = 0; entry_index < dictionary->bind_count; ++entry_index) {
		const cfg_bind_definition_t *bind = &dictionary->binds[entry_index];
		for (scope_index = 0; scope_index < bind->scope_count; ++scope_index) {
			CFGDictionary_ScopeFileId(bind->scopes[scope_index], 1, file_id, sizeof(file_id));
			applied.bind_nodes += CFGModel_MarkBindManagedInFile(model, file_id,
				bind->command, bind->case_sensitive);
		}
	}
	if (result) *result = applied;
	return 1;
}
