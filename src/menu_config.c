#include "quakedef.h"
#include "menu.h"
#include "menu_config.h"
#include "cfg_editor_dictionary.h"
#include "Ctrl.h"
#include "Ctrl_EditBox.h"
#include "Ctrl_KeyCapture.h"
#include "Ctrl_TextArea.h"
#include "textencoding.h"
#include "qsound.h"

extern int menuwidth, menuheight;
extern qbool m_entersound;
extern cvar_t menu_language;

#define CONFIG_PATH_PART "qw/config_editor"
#define CONFIG_VALUE_SIZE 256
#define CONFIG_PANEL_MAX_WIDTH 1280
#define CONFIG_TEXTAREA_HEIGHT 144

typedef enum config_item_kind_e {
	CONFIG_ITEM_SECTION,
	CONFIG_ITEM_CLASS,
	CONFIG_ITEM_SETTING,
	CONFIG_ITEM_BIND,
	CONFIG_ITEM_MISC,
	CONFIG_ITEM_TEXTAREA
} config_item_kind_t;

typedef struct config_layout_item_s {
	config_item_kind_t kind;
	const char *file_id;
	int data_index;
	int auxiliary;
	int content_y;
	int height;
} config_layout_item_t;

typedef struct config_setting_draft_s {
	const cfg_setting_definition_t *definition;
	char file_id[64];
	char value[CONFIG_VALUE_SIZE];
} config_setting_draft_t;

typedef struct config_bind_draft_s {
	const cfg_bind_definition_t *definition;
	char file_id[64];
	key_capture_control_t control;
} config_bind_draft_t;

typedef struct config_text_draft_s {
	char file_id[64];
	textarea_control_t area;
	qbool expanded;
} config_text_draft_t;

typedef struct config_menu_s {
	qbool loaded;
	char error[512];
	cfg_editor_model_t model;
	cfg_editor_dictionary_t dictionary;
	config_setting_draft_t *settings;
	size_t setting_count;
	config_bind_draft_t *binds;
	size_t bind_count;
	config_text_draft_t *texts;
	size_t text_count;
	int cursor;
	int scroll;
	qbool editing;
	qbool textarea_editing;
	CEditBox editbox;
	qbool section_open[4];
	qbool class_open[9];
	config_layout_item_t layout[1024];
	int layout_count;
	int content_height;
	int viewport_top;
	int viewport_bottom;
	char notice[256];
} config_menu_t;

static config_menu_t config_menu;

static const char *config_sections_ru[] = {
	"Основные настройки", "Бинды", "Настройки классов", "Настройки HUD"
};

static const char *config_sections_en[] = {
	"Main Settings", "Binds", "Class Settings", "HUD Settings"
};

static const char *config_classes_en[] = {
	"Scout", "Sniper", "Soldier", "Demoman", "Medic",
	"HWGuy", "Pyro", "Spy", "Engineer"
};

static const char *config_classes_ru[] = {
	"Скаут", "Снайпер", "Солдат", "Подрывник", "Медик",
	"Пулемётчик", "Пиротехник", "Шпион", "Инженер"
};

static const char *config_class_ids[] = {
	"class_scout", "class_sniper", "class_soldier", "class_demoman", "class_medic",
	"class_hwguy", "class_pyro", "class_spy", "class_engineer"
};

static qbool Config_UseEnglish(void)
{
	size_t i;
	for (i = 0; i < config_menu.setting_count; ++i) {
		const config_setting_draft_t *draft = &config_menu.settings[i];
		if (draft->definition && draft->definition->name &&
			!strcmp(draft->definition->name, "menu_language"))
			return !strcasecmp(draft->value, "English");
	}
	return !strcasecmp(menu_language.string, "English");
}

static const char *Config_Text(const char *russian, const char *english)
{
	return Config_UseEnglish() ? english : russian;
}

static const char *Config_SettingLabel(const cfg_setting_definition_t *definition)
{
	return Config_UseEnglish() ? definition->label_en : definition->label;
}

static const char *Config_SettingDescription(const cfg_setting_definition_t *definition)
{
	return Config_UseEnglish() ? definition->description_en : definition->description;
}

static const char *Config_BindLabel(const cfg_bind_definition_t *definition)
{
	return Config_UseEnglish() ? definition->label_en : definition->label;
}

static const char *Config_BindDescription(const cfg_bind_definition_t *definition)
{
	return Config_UseEnglish() ? definition->description_en : definition->description;
}

static void Config_DrawUTF8(int x, int y, const char *text, qbool active, int max_chars)
{
	wchar wide[512];
	int input = 0, output = 0, length;

	if (!text) text = "";
	length = (int)strlen(text);
	while (input < length && output < (int)(sizeof(wide) / sizeof(wide[0])) - 1 &&
		(max_chars <= 0 || output < max_chars)) {
		int initial = input;
		wchar decoded = TextEncodingDecodeUTF8((char *)text, &input);
		if (!decoded && text[initial]) {
			decoded = (unsigned char)text[initial];
			input = initial;
		}
		wide[output++] = decoded;
		++input;
	}
	wide[output] = 0;
	Draw_ConsoleString(x, y, wide, NULL, 0, active, 1, false);
}

static void Config_DrawUTF8Color(int x, int y, const char *text, color_t color, int max_chars)
{
	wchar wide[512];
	clrinfo_t colors[512];
	int input = 0, output = 0, length;

	if (!text) text = "";
	length = (int)strlen(text);
	while (input < length && output < (int)(sizeof(wide) / sizeof(wide[0])) - 1 &&
		(max_chars <= 0 || output < max_chars)) {
		int initial = input;
		wchar decoded = TextEncodingDecodeUTF8((char *)text, &input);
		if (!decoded && text[initial]) {
			decoded = (unsigned char)text[initial];
			input = initial;
		}
		wide[output] = decoded;
		colors[output].c = color;
		colors[output].i = output;
		++output;
		++input;
	}
	wide[output] = 0;
	Draw_ConsoleString(x, y, wide, colors, output, false, 1, false);
}

static void Config_DrawDecoratedValue(int x, int y, const char *text, int max_chars)
{
	wchar wide[512];
	int input = 0, output = 0, length;

	if (!text) text = "";
	max_chars = bound(3, max_chars, (int)(sizeof(wide) / sizeof(wide[0])) - 1);
	wide[output++] = 0x10;
	length = (int)strlen(text);
	while (input < length && output < max_chars - 1) {
		int initial = input;
		wchar decoded = TextEncodingDecodeUTF8((char *)text, &input);
		if (!decoded && text[initial]) {
			decoded = (unsigned char)text[initial];
			input = initial;
		}
		if (decoded >= '0' && decoded <= '9') decoded = decoded - '0' + 0x12;
		wide[output++] = decoded;
		++input;
	}
	wide[output++] = 0x11;
	wide[output] = 0;
	Draw_ConsoleString(x, y, wide, NULL, 0, false, 1, false);
}

static int Config_PanelWidth(void)
{
	return min(CONFIG_PANEL_MAX_WIDTH, max(320, vid.width - OPTPADDING * 2 - 12));
}

static int Config_PanelLeft(void)
{
	return (vid.width - Config_PanelWidth()) / 2;
}

static int Config_ValueColumnX(void)
{
	return Config_PanelLeft() + Config_PanelWidth() / 2 + LETTERWIDTH * 2;
}

static int Config_UTF8Length(const char *text)
{
	int input = 0, characters = 0, length;
	if (!text) return 0;
	length = (int)strlen(text);
	while (input < length) {
		TextEncodingDecodeUTF8((char *)text, &input);
		++input;
		++characters;
	}
	return characters;
}

static qbool Config_StringEqual(const char *left, const char *right, qbool case_sensitive)
{
	if (!left || !right) return false;
	return case_sensitive ? !strcmp(left, right) : !strcasecmp(left, right);
}

static qbool Config_ScopeToFile(const char *scope, qbool bind_scope, char *file_id, size_t size)
{
	int result;
	if (bind_scope && !strcmp(scope, "global")) {
		result = snprintf(file_id, size, "binds");
	}
	else if (!bind_scope && (!strcmp(scope, "main") || !strcmp(scope, "hud"))) {
		result = snprintf(file_id, size, "%s", scope);
	}
	else if (!strncmp(scope, "class:", 6) && scope[6]) {
		result = snprintf(file_id, size, "class_%s", scope + 6);
	}
	else return false;
	return result >= 0 && (size_t)result < size;
}

static void Config_FreeSession(void)
{
	size_t i;
	for (i = 0; i < config_menu.text_count; ++i) {
		CTextArea_Free(&config_menu.texts[i].area);
	}
	Q_free(config_menu.texts);
	Q_free(config_menu.binds);
	Q_free(config_menu.settings);
	CFGDictionary_Free(&config_menu.dictionary);
	CFGModel_Free(&config_menu.model);
	memset(&config_menu, 0, sizeof(config_menu));
}

static qbool Config_BuildMisc(config_text_draft_t *text, const cfg_model_file_t *file)
{
	cfg_source_ref_t *references = NULL;
	textarea_source_line_t *sources = NULL;
	char *buffer = NULL;
	size_t reference_count, source_count = 0, source_capacity = 0;
	size_t length = 0, capacity = 1, i;

	reference_count = CFGModel_CollectMisc(&config_menu.model, file->id, 1, &references);
	buffer = Q_malloc(capacity);
	if (!buffer) goto fail;
	buffer[0] = '\0';

	for (i = 0; i < reference_count; ++i) {
		size_t byte_index;
		const cfg_source_ref_t *reference = &references[i];
		if (length + reference->raw_length + 1 > capacity) {
			char *resized;
			while (length + reference->raw_length + 1 > capacity) capacity *= 2;
			resized = Q_realloc(buffer, capacity);
			if (!resized) goto fail;
			buffer = resized;
		}
		memcpy(buffer + length, reference->raw, reference->raw_length);
		length += reference->raw_length;
		buffer[length] = '\0';

		for (byte_index = 0; byte_index < reference->raw_length; ++byte_index) {
			if (byte_index == 0 || reference->raw[byte_index - 1] == '\n') {
				textarea_source_line_t *resized;
				if (source_count == source_capacity) {
					source_capacity = source_capacity ? source_capacity * 2 : 16;
					resized = Q_realloc(sources, source_capacity * sizeof(*sources));
					if (!resized) goto fail;
					sources = resized;
				}
				memset(&sources[source_count], 0, sizeof(sources[source_count]));
				strlcpy(sources[source_count].file, file->path, sizeof(sources[source_count].file));
				sources[source_count].node_id = reference->node_id;
				sources[source_count].source_line = reference->line_start;
				++source_count;
			}
		}
	}

	strlcpy(text->file_id, file->id, sizeof(text->file_id));
	CTextArea_Init(&text->area, 80, 16);
	text->area.show_source_gutter = true;
	text->area.use_console_font = true;
	if (!CTextArea_SetText(&text->area, buffer, length) ||
		!CTextArea_SetSources(&text->area, sources, source_count)) goto fail_initialized;
	Q_free(buffer);
	Q_free(sources);
	free(references);
	return true;

fail_initialized:
	CTextArea_Free(&text->area);
fail:
	Q_free(buffer);
	Q_free(sources);
	free(references);
	return false;
}

static qbool Config_BuildHud(config_text_draft_t *text, const cfg_model_file_t *file)
{
	unsigned char *data = NULL;
	size_t length = 0;
	strlcpy(text->file_id, file->id, sizeof(text->file_id));
	CTextArea_Init(&text->area, 80, 16);
	text->area.use_console_font = true;
	if (!CFGModel_SerializeFile(&config_menu.model, file->id, &data, &length) ||
		!CTextArea_SetText(&text->area, (const char *)data, length)) {
		free(data);
		CTextArea_Free(&text->area);
		return false;
	}
	free(data);
	return true;
}

static qbool Config_BuildDrafts(void)
{
	size_t i, j, setting_count = 0, bind_count = 0;

	for (i = 0; i < config_menu.dictionary.setting_count; ++i)
		setting_count += config_menu.dictionary.settings[i].scope_count;
	for (i = 0; i < config_menu.dictionary.bind_count; ++i)
		bind_count += config_menu.dictionary.binds[i].scope_count;

	config_menu.settings = Q_calloc(setting_count, sizeof(*config_menu.settings));
	config_menu.binds = Q_calloc(bind_count, sizeof(*config_menu.binds));
	config_menu.texts = Q_calloc(config_menu.model.file_count, sizeof(*config_menu.texts));
	if (!config_menu.settings || !config_menu.binds || !config_menu.texts) return false;

	for (i = 0; i < config_menu.dictionary.setting_count; ++i) {
		const cfg_setting_definition_t *definition = &config_menu.dictionary.settings[i];
		for (j = 0; j < definition->scope_count; ++j) {
			config_setting_draft_t *draft = &config_menu.settings[config_menu.setting_count++];
			cfg_setting_result_t resolved;
			draft->definition = definition;
			if (!Config_ScopeToFile(definition->scopes[j], false, draft->file_id, sizeof(draft->file_id))) return false;
			if (CFGModel_ResolveSetting(&config_menu.model, draft->file_id, definition->storage_kind,
				definition->name, definition->on_command, definition->off_command, &resolved)) {
				strlcpy(draft->value, resolved.value ? resolved.value : "", sizeof(draft->value));
			}
		}
	}

	for (i = 0; i < config_menu.dictionary.bind_count; ++i) {
		const cfg_bind_definition_t *definition = &config_menu.dictionary.binds[i];
		for (j = 0; j < definition->scope_count; ++j) {
			config_bind_draft_t *draft = &config_menu.binds[config_menu.bind_count++];
			cfg_model_file_t *file;
			int keys[KEY_CAPTURE_MAX_KEYS], key_count = 0;
			size_t node_index;
			draft->definition = definition;
			if (!Config_ScopeToFile(definition->scopes[j], true, draft->file_id, sizeof(draft->file_id))) return false;
			CKeyCapture_Init(&draft->control, definition->max_keys);
			file = CFGModel_FindFile(&config_menu.model, draft->file_id);
			if (file) for (node_index = 0; node_index < file->document.node_count; ++node_index) {
				cfg_node_t *node = &file->document.nodes[node_index];
				if (node->kind == CFG_NODE_BIND && key_count < draft->control.max_keys &&
					Config_StringEqual(node->value, definition->command, definition->case_sensitive)) {
					int key = Key_StringToKeynum(node->name);
					if (key >= 0 && key < KEY_MAX_KEYS) keys[key_count++] = key;
				}
			}
			CKeyCapture_SetKeys(&draft->control, keys, key_count);
		}
	}

	for (i = 0; i < config_menu.model.file_count; ++i) {
		const cfg_model_file_t *file = &config_menu.model.files[i];
		qbool success = !strcmp(file->id, "hud") ?
			Config_BuildHud(&config_menu.texts[config_menu.text_count], file) :
			Config_BuildMisc(&config_menu.texts[config_menu.text_count], file);
		if (!success) return false;
		++config_menu.text_count;
	}
	return true;
}

static qbool Config_LoadSession(void)
{
	char manifest[MAX_OSPATH], settings[MAX_OSPATH], binds[MAX_OSPATH];
	char error[512] = { 0 };
	cfg_dictionary_apply_result_t applied;

	Config_FreeSession();
	CFGModel_Init(&config_menu.model);
	CFGDictionary_Init(&config_menu.dictionary);
	snprintf(manifest, sizeof(manifest), "%s/%s/managed_files.json", com_basedir, CONFIG_PATH_PART);
	snprintf(settings, sizeof(settings), "%s/%s/dict_settings.json", com_basedir, CONFIG_PATH_PART);
	snprintf(binds, sizeof(binds), "%s/%s/dict_binds.json", com_basedir, CONFIG_PATH_PART);

	if (!CFGModel_LoadManifest(&config_menu.model, manifest, com_basedir, error, sizeof(error)) ||
		!CFGDictionary_Load(&config_menu.dictionary, settings, binds, error, sizeof(error)) ||
		!CFGDictionary_ApplyToModel(&config_menu.dictionary, &config_menu.model, &applied, error, sizeof(error)) ||
		!Config_BuildDrafts()) {
		strlcpy(config_menu.error, error[0] ? error : "Unable to prepare the config editor draft", sizeof(config_menu.error));
		config_menu.loaded = false;
		return false;
	}
	config_menu.loaded = true;
	config_menu.section_open[0] = true;
	return true;
}

static config_text_draft_t *Config_FindText(const char *file_id)
{
	size_t i;
	for (i = 0; i < config_menu.text_count; ++i)
		if (!strcmp(config_menu.texts[i].file_id, file_id)) return &config_menu.texts[i];
	return NULL;
}

static int Config_SettingCount(const char *file_id)
{
	int count = 0;
	size_t i;
	for (i = 0; i < config_menu.setting_count; ++i)
		count += !strcmp(config_menu.settings[i].file_id, file_id);
	return count;
}

static config_setting_draft_t *Config_SettingAt(const char *file_id, int wanted)
{
	size_t i;
	for (i = 0; i < config_menu.setting_count; ++i) {
		if (!strcmp(config_menu.settings[i].file_id, file_id) && wanted-- == 0) return &config_menu.settings[i];
	}
	return NULL;
}

static int Config_BindCount(const char *file_id)
{
	int count = 0;
	size_t i;
	for (i = 0; i < config_menu.bind_count; ++i)
		count += !strcmp(config_menu.binds[i].file_id, file_id);
	return count;
}

static config_bind_draft_t *Config_BindAt(const char *file_id, int wanted)
{
	size_t i;
	for (i = 0; i < config_menu.bind_count; ++i) {
		if (!strcmp(config_menu.binds[i].file_id, file_id) && wanted-- == 0) return &config_menu.binds[i];
	}
	return NULL;
}

static const char *Config_NodeLineEnding(const cfg_node_t *node)
{
	if (node && node->raw_length >= 2 &&
		node->raw[node->raw_length - 2] == '\r' && node->raw[node->raw_length - 1] == '\n') return "\r\n";
	if (node && node->raw_length && node->raw[node->raw_length - 1] == '\r') return "\r";
	if (node && node->raw_length && node->raw[node->raw_length - 1] == '\n') return "\n";
	return "";
}

static qbool Config_AppendEscaped(char *output, size_t output_size, size_t *length, const char *value)
{
	const unsigned char *cursor = (const unsigned char *)(value ? value : "");
	while (*cursor) {
		if ((*cursor == '"' || *cursor == '\\') && *length + 1 < output_size)
			output[(*length)++] = '\\';
		if (*length + 1 >= output_size) return false;
		output[(*length)++] = (char)*cursor++;
	}
	output[*length] = '\0';
	return true;
}

static qbool Config_ReplaceSettingDraft(config_setting_draft_t *draft)
{
	cfg_setting_result_t resolved;
	char raw[1024];
	size_t length = 0;
	const char *prefix = "";
	const char *line_ending;
	const cfg_setting_definition_t *definition = draft->definition;

	if (!CFGModel_ResolveSetting(&config_menu.model, draft->file_id,
		definition->storage_kind, definition->name, definition->on_command,
		definition->off_command, &resolved) || !resolved.node || resolved.inherited) return true;
	if (resolved.value && !strcmp(resolved.value, draft->value)) return true;
	line_ending = Config_NodeLineEnding(resolved.node);
	if (definition->storage_kind == CFG_STORAGE_SET) prefix = "set ";
	else if (definition->storage_kind == CFG_STORAGE_SETINFO) prefix = "setinfo ";
	if (definition->storage_kind == CFG_STORAGE_COMMAND_TOGGLE) {
		const char *command = !strcmp(draft->value, "0") ? definition->off_command : definition->on_command;
		int written = snprintf(raw, sizeof(raw), "%s%s", command, line_ending);
		if (written < 0 || (size_t)written >= sizeof(raw)) return false;
		length = (size_t)written;
	}
	else {
		int written = snprintf(raw, sizeof(raw), "%s%s \"", prefix, definition->name);
		if (written < 0 || (size_t)written >= sizeof(raw)) return false;
		length = (size_t)written;
		if (!Config_AppendEscaped(raw, sizeof(raw), &length, draft->value) ||
			length + strlen(line_ending) + 2 > sizeof(raw)) return false;
		raw[length++] = '"';
		memcpy(raw + length, line_ending, strlen(line_ending));
		length += strlen(line_ending);
		raw[length] = '\0';
	}
	return CFGDoc_ReplaceNode(&resolved.file->document, resolved.node->id,
		(const unsigned char *)raw, length) ? (resolved.file->dirty = 1, true) : false;
}

static qbool Config_BufferAppend(char **buffer, size_t *length, size_t *capacity,
	const char *data, size_t data_length)
{
	char *resized;
	if (*length + data_length + 1 > *capacity) {
		size_t next = *capacity ? *capacity : 64;
		while (*length + data_length + 1 > next) next *= 2;
		resized = Q_realloc(*buffer, next);
		if (!resized) return false;
		*buffer = resized;
		*capacity = next;
	}
	if (data_length) memcpy(*buffer + *length, data, data_length);
	*length += data_length;
	(*buffer)[*length] = '\0';
	return true;
}

static qbool Config_ApplyMiscDraft(config_text_draft_t *draft)
{
	cfg_source_ref_t *references = NULL;
	size_t reference_count, reference_index, row = 0, position = 0;
	const char *text;
	size_t text_length;

	reference_count = CFGModel_CollectMisc(&config_menu.model, draft->file_id, 1, &references);
	text = CTextArea_Text(&draft->area, &text_length);
	if (!reference_count) {
		free(references);
		return text_length == 0;
	}
	for (reference_index = 0; reference_index < reference_count; ++reference_index) {
		char *replacement = NULL;
		size_t replacement_length = 0, replacement_capacity = 0;
		position = 0;
		row = 0;
		while (position < text_length || (position == 0 && !text_length)) {
			size_t end = position;
			while (end < text_length && text[end] != '\r' && text[end] != '\n') ++end;
			if (end < text_length && text[end] == '\r' && end + 1 < text_length && text[end + 1] == '\n') end += 2;
			else if (end < text_length) ++end;
			if (row < draft->area.source_count &&
				draft->area.sources[row].node_id == references[reference_index].node_id &&
				!Config_BufferAppend(&replacement, &replacement_length, &replacement_capacity,
					text + position, end - position)) {
				Q_free(replacement); free(references); return false;
			}
			if (end <= position) break;
			position = end;
			++row;
		}
		if (replacement_length == references[reference_index].raw_length &&
			(!replacement_length || !memcmp(replacement, references[reference_index].raw, replacement_length))) {
			Q_free(replacement);
			continue;
		}
		if (!CFGModel_ReplaceSource(&config_menu.model, &references[reference_index],
			(const unsigned char *)replacement, replacement_length)) {
			Q_free(replacement); free(references); return false;
		}
		Q_free(replacement);
	}
	free(references);
	return true;
}

static qbool Config_ApplyTextDrafts(void)
{
	size_t i;
	for (i = 0; i < config_menu.text_count; ++i) {
		config_text_draft_t *draft = &config_menu.texts[i];
		size_t length;
		const char *text = CTextArea_Text(&draft->area, &length);
		if (!strcmp(draft->file_id, "hud")) {
			unsigned char *current = NULL;
			size_t current_length = 0;
			if (!CFGModel_SerializeFile(&config_menu.model, draft->file_id, &current, &current_length)) return false;
			if (current_length != length || (length && memcmp(current, text, length))) {
				free(current);
				if (!CFGModel_ReplaceFileContents(&config_menu.model, draft->file_id,
					(const unsigned char *)text, length)) return false;
			}
			else free(current);
		}
		else if (!Config_ApplyMiscDraft(draft)) return false;
	}
	return true;
}

static qbool Config_ReplaceBindDraft(config_bind_draft_t *draft)
{
	cfg_model_file_t *file = CFGModel_FindFile(&config_menu.model, draft->file_id);
	unsigned int first_id = 0;
	size_t i, built_length = 0, built_capacity = 0;
	char *built = NULL, *combined = NULL;
	size_t combined_length = 0, combined_capacity = 0;
	int existing_keys[KEY_CAPTURE_MAX_KEYS + 1];
	size_t existing_count = 0;
	const char *ending = "\r\n";

	if (!file) return false;
	for (i = 0; i < file->document.node_count; ++i) {
		cfg_node_t *node = &file->document.nodes[i];
		if (node->kind == CFG_NODE_BIND &&
			Config_StringEqual(node->value, draft->definition->command, draft->definition->case_sensitive)) {
			int key = Key_StringToKeynum(node->name);
			if (existing_count < sizeof(existing_keys) / sizeof(existing_keys[0])) existing_keys[existing_count] = key;
			++existing_count;
			if (!first_id) {
				first_id = node->id;
				if (*Config_NodeLineEnding(node)) ending = Config_NodeLineEnding(node);
			}
		}
	}
	if (existing_count == (size_t)draft->control.key_count &&
		existing_count <= sizeof(existing_keys) / sizeof(existing_keys[0])) {
		qbool same = true;
		for (i = 0; i < existing_count; ++i) same &= existing_keys[i] == draft->control.keys[i];
		if (same) return true;
	}
	for (i = 0; i < (size_t)draft->control.key_count; ++i) {
		char line[1024];
		size_t length = 0;
		int written = snprintf(line, sizeof(line), "bind %s \"",
			Key_KeynumToString(draft->control.keys[i]));
		if (written < 0 || (size_t)written >= sizeof(line)) goto fail;
		length = (size_t)written;
		if (!Config_AppendEscaped(line, sizeof(line), &length, draft->definition->command) ||
			length + strlen(ending) + 2 > sizeof(line)) goto fail;
		line[length++] = '"';
		memcpy(line + length, ending, strlen(ending)); length += strlen(ending);
		if (!Config_BufferAppend(&built, &built_length, &built_capacity, line, length)) goto fail;
	}
	if (first_id) {
		qbool first = true;
		for (i = 0; i < file->document.node_count; ++i) {
			cfg_node_t *node = &file->document.nodes[i];
			if (node->kind == CFG_NODE_BIND &&
				Config_StringEqual(node->value, draft->definition->command, draft->definition->case_sensitive)) {
				if (!CFGDoc_ReplaceNode(&file->document, node->id,
					(const unsigned char *)(first ? built : NULL), first ? built_length : 0)) goto fail;
				first = false;
			}
		}
	}
	else if (built_length) {
		cfg_node_t *last = file->document.node_count ? &file->document.nodes[file->document.node_count - 1] : NULL;
		if (!last) goto fail;
		if (!Config_BufferAppend(&combined, &combined_length, &combined_capacity,
			(const char *)last->raw, last->raw_length)) goto fail_combined;
		if (combined_length && combined[combined_length - 1] != '\n' && combined[combined_length - 1] != '\r' &&
			!Config_BufferAppend(&combined, &combined_length, &combined_capacity, ending, strlen(ending))) goto fail_combined;
		if (!Config_BufferAppend(&combined, &combined_length, &combined_capacity, built, built_length) ||
			!CFGDoc_ReplaceNode(&file->document, last->id, (const unsigned char *)combined, combined_length)) goto fail_combined;
		Q_free(combined);
		combined = NULL;
	}
	file->dirty = 1;
	Q_free(built);
	return true;

fail_combined:
	Q_free(combined);
fail:
	Q_free(built);
	return false;
}

static qbool Config_WriteDirtyFiles(void)
{
	size_t i;
	for (i = 0; i < config_menu.model.file_count; ++i) {
		cfg_model_file_t *file = &config_menu.model.files[i];
		unsigned char *data = NULL;
		size_t length = 0;
		char path[MAX_OSPATH];
		if (!file->dirty) continue;
		if (!CFGModel_SerializeFile(&config_menu.model, file->id, &data, &length)) return false;
		if (snprintf(path, sizeof(path), "%s/fortress/%s", com_basedir, file->path) < 0 ||
			strlen(path) >= sizeof(path) - 1) {
			free(data);
			return false;
		}
		if (length > INT_MAX || !FS_WriteFile_2(path, data, (int)length)) {
			free(data);
			return false;
		}
		free(data);
	}
	return true;
}

static qbool Config_SaveSession(void)
{
	size_t i;
	qbool english;
	if (!Config_ApplyTextDrafts()) goto fail;
	for (i = 0; i < config_menu.setting_count; ++i)
		if (!Config_ReplaceSettingDraft(&config_menu.settings[i])) goto fail;
	for (i = 0; i < config_menu.bind_count; ++i)
		if (!Config_ReplaceBindDraft(&config_menu.binds[i])) goto fail;
	if (!Config_WriteDirtyFiles()) goto fail;
	english = Config_UseEnglish();
	Cvar_Set(&menu_language, english ? "English" : "Russian");
	if (!Config_LoadSession()) return false;
	strlcpy(config_menu.notice, english ? "Changes saved" : "Изменения сохранены", sizeof(config_menu.notice));
	return true;

fail:
	strlcpy(config_menu.notice, Config_Text("Ошибка сохранения конфигурации", "Unable to save configuration"), sizeof(config_menu.notice));
	return false;
}

static void Config_DrawHelpBox(const char *text)
{
	int height = 40;
	int y = vid.height - OPTPADDING - height;
	int left = Config_PanelLeft();
	int width = Config_PanelWidth();
	UI_DrawBox(left, y, width, height);
	Config_DrawUTF8(left + LETTERWIDTH, y + LETTERHEIGHT, text, false,
		max(20, width / LETTERWIDTH - 4));
	Config_DrawUTF8(left + LETTERWIDTH, y + LETTERHEIGHT * 3,
		Config_Text("Enter: изменить/открыть   Ctrl+S: сохранить   Ctrl+R: перечитать   Esc: назад",
			"Enter: edit/open   Ctrl+S: save   Ctrl+R: reload   Esc: back"), false,
		max(20, width / LETTERWIDTH - 4));
	if (config_menu.notice[0])
		Config_DrawUTF8(left + width - min(width / 2, Config_UTF8Length(config_menu.notice) * LETTERWIDTH),
			y + LETTERHEIGHT, config_menu.notice, true, max(20, width / LETTERWIDTH / 2));
}

static void Config_DrawLoadError(void)
{
	int left = Config_PanelLeft();
	int width = Config_PanelWidth();
	Config_DrawUTF8(left, OPTPADDING, Config_Text("Конфигурация", "Config"), true, 40);
	UI_DrawBox(left, 24, width, 64);
	Config_DrawUTF8(left + 8, 32,
		Config_Text("Не удалось загрузить файлы конфигурации:", "Unable to load config files:"), true, 80);
	UI_Print(left + 8, 48, config_menu.error, false);
	Config_DrawUTF8(left + 8, 72,
		Config_Text("Enter: повторить   Esc: назад", "Enter: retry   Esc: back"), false, 60);
}

static void Config_AddLayout(config_item_kind_t kind, const char *file_id,
	int data_index, int auxiliary, int height)
{
	config_layout_item_t *item;
	if (config_menu.layout_count >= (int)(sizeof(config_menu.layout) / sizeof(config_menu.layout[0]))) return;
	item = &config_menu.layout[config_menu.layout_count++];
	item->kind = kind;
	item->file_id = file_id;
	item->data_index = data_index;
	item->auxiliary = auxiliary;
	item->content_y = config_menu.content_height;
	item->height = height;
	config_menu.content_height += height;
}

static void Config_AddFileLayout(const char *file_id, qbool include_settings, qbool include_binds)
{
	config_text_draft_t *text = Config_FindText(file_id);
	int i;
	if (include_settings) for (i = 0; i < Config_SettingCount(file_id); ++i) {
		config_setting_draft_t *draft = Config_SettingAt(file_id, i);
		Config_AddLayout(CONFIG_ITEM_SETTING, file_id, i, 0,
			draft && !strcmp(draft->definition->id, "menu_language") ? 20 : 10);
	}
	if (include_binds) for (i = 0; i < Config_BindCount(file_id); ++i)
		Config_AddLayout(CONFIG_ITEM_BIND, file_id, i, 0, 10);
	Config_AddLayout(CONFIG_ITEM_MISC, file_id, 0, 0, 12);
	if (text && text->expanded) Config_AddLayout(CONFIG_ITEM_TEXTAREA, file_id, 0, 0, CONFIG_TEXTAREA_HEIGHT);
}

static void Config_BuildLayout(void)
{
	int section, class_index;
	config_menu.layout_count = 0;
	config_menu.content_height = 0;
	for (section = 0; section < 4; ++section) {
		Config_AddLayout(CONFIG_ITEM_SECTION, NULL, 0, section, 16);
		if (!config_menu.section_open[section]) continue;
		if (section == 0) Config_AddFileLayout("main", true, false);
		else if (section == 1) Config_AddFileLayout("binds", false, true);
		else if (section == 2) {
			for (class_index = 0; class_index < 9; ++class_index) {
				Config_AddLayout(CONFIG_ITEM_CLASS, config_class_ids[class_index], 0, class_index, 14);
				if (config_menu.class_open[class_index])
					Config_AddFileLayout(config_class_ids[class_index], true, true);
			}
		}
		else {
			Config_AddLayout(CONFIG_ITEM_TEXTAREA, "hud", 0, 0, CONFIG_TEXTAREA_HEIGHT);
		}
	}
	if (config_menu.layout_count)
		config_menu.cursor = bound(0, config_menu.cursor, config_menu.layout_count - 1);
	else config_menu.cursor = 0;
}

static config_layout_item_t *Config_SelectedItem(void)
{
	Config_BuildLayout();
	return config_menu.layout_count ? &config_menu.layout[config_menu.cursor] : NULL;
}

static void Config_KeepCursorVisible(void)
{
	config_layout_item_t *item;
	int viewport_height, max_scroll;
	if (!config_menu.layout_count) return;
	item = &config_menu.layout[config_menu.cursor];
	viewport_height = max(1, config_menu.viewport_bottom - config_menu.viewport_top);
	if (item->content_y < config_menu.scroll) config_menu.scroll = item->content_y;
	if (item->content_y + item->height > config_menu.scroll + viewport_height)
		config_menu.scroll = item->content_y + item->height - viewport_height;
	max_scroll = max(0, config_menu.content_height - viewport_height);
	config_menu.scroll = bound(0, config_menu.scroll, max_scroll);
}

static void Config_DrawSettingValue(config_setting_draft_t *draft, int x, int y, qbool active, int max_chars)
{
	const cfg_setting_definition_t *definition = draft->definition;
	const char *value = draft->value;

	if (active && config_menu.editing) {
		CEditBox_Draw(&config_menu.editbox, x, y, true);
		return;
	}
	if (definition->widget_type == CFG_WIDGET_CHECKBOX) {
		value = !strcmp(draft->value, definition->checked_value) ?
			Config_Text("Да", "Yes") : Config_Text("Нет", "No");
	}
	else if (definition->widget_type == CFG_WIDGET_SELECT) {
		size_t i;
		for (i = 0; i < definition->option_count; ++i)
			if (!strcmp(value, definition->options[i].value)) value = Config_UseEnglish() ?
				definition->options[i].label_en : definition->options[i].label;
	}
	Config_DrawDecoratedValue(x, y, value, max_chars);
}

static void Config_DrawTextArea(const config_layout_item_t *item, int x, int y, int available_width, qbool active)
{
	config_text_draft_t *draft = Config_FindText(item->file_id);
	int box_width = available_width;
	int box_height = item->height - 8;
	if (!draft) return;
	UI_DrawBox(x, y + 2, box_width, box_height);
	if (active)
		Draw_AlphaRectangleRGB(x, y + 2, box_width, box_height, 1, false,
			RGBA_TO_COLOR(255, 112, 32, 255));
	draft->area.width = max(24, (int)(box_width / CTextArea_CharacterWidth(&draft->area)) - 2);
	draft->area.height = max(3, box_height / 8 - 2);
	CTextArea_Draw(&draft->area, x + 8, y + 10, active && config_menu.textarea_editing);
	if (!draft->area.length)
		Config_DrawUTF8(x + 8, y + 10,
			Config_Text("(нет строк для отображения)", "(no lines to display)"), false, 40);
}

static void Config_DrawLayoutItem(const config_layout_item_t *item, int index, int y)
{
	qbool active = index == config_menu.cursor;
	int left = Config_PanelLeft();
	int width = Config_PanelWidth();
	int value_x = Config_ValueColumnX();
	int indent = 0;
	int item_x;
	char label[96];

	if (item->kind == CONFIG_ITEM_TEXTAREA) {
		Config_DrawTextArea(item, value_x, y, left + width - value_x, active);
		return;
	}
	if (item->kind == CONFIG_ITEM_CLASS) indent = 12;
	else if (item->kind != CONFIG_ITEM_SECTION) indent = 28;
	item_x = (item->kind == CONFIG_ITEM_SETTING || item->kind == CONFIG_ITEM_BIND) ?
		left + indent : value_x + indent;
	if (active) UI_DrawGrayBox(item_x, y, left + width - item_x, item->height - 2);

	if (item->kind == CONFIG_ITEM_SECTION) {
		color_t color = active ? RGBA_TO_COLOR(255, 112, 32, 255) : RGBA_TO_COLOR(145, 92, 42, 255);
		snprintf(label, sizeof(label), "[%c] ", config_menu.section_open[item->auxiliary] ? '-' : '+');
		Config_DrawUTF8Color(value_x, y + 3, label, color, 4);
		Config_DrawUTF8Color(value_x + 4 * LETTERWIDTH, y + 3,
			(Config_UseEnglish() ? config_sections_en : config_sections_ru)[item->auxiliary],
			color, 40);
	}
	else if (item->kind == CONFIG_ITEM_CLASS) {
		color_t color = active ? RGBA_TO_COLOR(255, 112, 32, 255) : RGBA_TO_COLOR(205, 150, 82, 255);
		snprintf(label, sizeof(label), "[%c] ", config_menu.class_open[item->auxiliary] ? '-' : '+');
		Config_DrawUTF8Color(value_x + indent, y + 2, label, color, 4);
		Config_DrawUTF8Color(value_x + indent + 4 * LETTERWIDTH, y + 2,
			(Config_UseEnglish() ? config_classes_en : config_classes_ru)[item->auxiliary],
			color, 40);
	}
	else if (item->kind == CONFIG_ITEM_MISC) {
		config_text_draft_t *text = Config_FindText(item->file_id);
		color_t color = active ? RGBA_TO_COLOR(255, 112, 32, 255) : RGBA_TO_COLOR(205, 150, 82, 255);
		snprintf(label, sizeof(label), "[%c] ", text && text->expanded ? '-' : '+');
		Config_DrawUTF8Color(value_x + indent, y + 2, label, color, 4);
		Config_DrawUTF8Color(value_x + indent + 4 * LETTERWIDTH, y + 2,
			Config_Text("Остальные настройки...", "Other settings..."),
			color, 40);
	}
	else {
		int label_chars = max(12, (value_x - left - indent) / LETTERWIDTH - 3);
		if (item->kind == CONFIG_ITEM_SETTING) {
			config_setting_draft_t *draft = Config_SettingAt(item->file_id, item->data_index);
			const char *setting_label = Config_SettingLabel(draft->definition);
			int label_length = min(label_chars, Config_UTF8Length(setting_label));
			if (active)
				Config_DrawUTF8Color(value_x - label_length * LETTERWIDTH - LETTERWIDTH * 2, y,
					setting_label, RGBA_TO_COLOR(255, 112, 32, 255), label_chars);
			else Config_DrawUTF8(value_x - label_length * LETTERWIDTH - LETTERWIDTH * 2, y,
				setting_label, false, label_chars);
			Config_DrawSettingValue(draft, value_x, y, active,
				max(3, (left + width - value_x) / LETTERWIDTH));
		}
		else {
			config_bind_draft_t *draft = Config_BindAt(item->file_id, item->data_index);
			const char *bind_label = Config_BindLabel(draft->definition);
			int label_length = min(label_chars, Config_UTF8Length(bind_label));
			if (active)
				Config_DrawUTF8Color(value_x - label_length * LETTERWIDTH - LETTERWIDTH * 2, y,
					bind_label, RGBA_TO_COLOR(255, 112, 32, 255), label_chars);
			else Config_DrawUTF8(value_x - label_length * LETTERWIDTH - LETTERWIDTH * 2, y,
				bind_label, false, label_chars);
			CKeyCapture_Draw(&draft->control, value_x, y, 22, active);
		}
	}
}

static const char *Config_SelectedHelp(void)
{
	config_layout_item_t *item = config_menu.layout_count ? &config_menu.layout[config_menu.cursor] : NULL;
	if (!item) return "";
	if (item->kind == CONFIG_ITEM_SETTING)
		return Config_SettingDescription(Config_SettingAt(item->file_id, item->data_index)->definition);
	if (item->kind == CONFIG_ITEM_BIND)
		return Config_BindDescription(Config_BindAt(item->file_id, item->data_index)->definition);
	if (item->kind == CONFIG_ITEM_MISC || (item->kind == CONFIG_ITEM_TEXTAREA && strcmp(item->file_id, "hud")))
		return Config_Text("Неизвестные строки сохраняют исходный файл, положение и порядок.",
			"Unknown lines preserve their original file, position, and order.");
	if (item->kind == CONFIG_ITEM_TEXTAREA)
		return Config_Text("Полный hud.cfg. Enter включает редактирование текста, Esc возвращает управление странице.",
			"Complete hud.cfg. Enter enables text editing; Esc returns control to the page.");
	if (item->kind == CONFIG_ITEM_CLASS)
		return Config_Text("Настройки, бинды и остальные строки выбранного классового CFG.",
			"Settings, binds, and other lines from the selected class CFG.");
	return Config_Text("Раздел можно свернуть или развернуть клавишей Enter.",
		"Press Enter to collapse or expand the section.");
}

void Menu_Config_Draw(void)
{
	int i;
	int left, width, line_chars;
	char line[256];
	M_Unscale_Menu();
	if (!config_menu.loaded) {
		Config_DrawLoadError();
		return;
	}
	config_menu.viewport_top = OPTPADDING + LETTERHEIGHT * 2;
	config_menu.viewport_bottom = vid.height - OPTPADDING - 44;
	Config_BuildLayout();
	Config_KeepCursorVisible();
	left = Config_PanelLeft();
	width = Config_PanelWidth();
	line_chars = bound(2, width / LETTERWIDTH, (int)sizeof(line) - 1);
	UI_MakeLine(line, line_chars);
	Config_DrawUTF8(left, OPTPADDING, Config_Text("Конфигурация", "Config"), true, 40);
	UI_Print(left, OPTPADDING + LETTERHEIGHT, line, false);
	for (i = 0; i < config_menu.layout_count; ++i) {
		config_layout_item_t *item = &config_menu.layout[i];
		int y = config_menu.viewport_top + item->content_y - config_menu.scroll;
		if (y < config_menu.viewport_top || y + item->height > config_menu.viewport_bottom) continue;
		Config_DrawLayoutItem(item, i, y);
	}
	Config_DrawHelpBox(Config_SelectedHelp());
}

static void Config_AdjustSetting(config_setting_draft_t *draft, int direction)
{
	const cfg_setting_definition_t *definition = draft->definition;
	if (definition->widget_type == CFG_WIDGET_CHECKBOX) {
		const char *next = !strcmp(draft->value, definition->checked_value) ?
			definition->unchecked_value : definition->checked_value;
		strlcpy(draft->value, next, sizeof(draft->value));
	}
	else if (definition->widget_type == CFG_WIDGET_SELECT && definition->option_count) {
		size_t i, current = 0;
		for (i = 0; i < definition->option_count; ++i)
			if (!strcmp(draft->value, definition->options[i].value)) current = i;
		current = (current + definition->option_count + direction) % definition->option_count;
		strlcpy(draft->value, definition->options[current].value, sizeof(draft->value));
	}
}

static void Config_BeginEdit(config_setting_draft_t *draft)
{
	int maximum = draft->definition->max_length > 0 ? draft->definition->max_length : MAX_EDITTEXT;
	CEditBox_Init(&config_menu.editbox, 22, min(maximum, MAX_EDITTEXT));
	strlcpy(config_menu.editbox.text, draft->value, sizeof(config_menu.editbox.text));
	config_menu.editbox.pos = strlen(config_menu.editbox.text);
	config_menu.editing = true;
}

static void Config_SetExpanded(config_layout_item_t *item, qbool expanded)
{
	if (item->kind == CONFIG_ITEM_SECTION) config_menu.section_open[item->auxiliary] = expanded;
	else if (item->kind == CONFIG_ITEM_CLASS) config_menu.class_open[item->auxiliary] = expanded;
	else if (item->kind == CONFIG_ITEM_MISC) {
		config_text_draft_t *text = Config_FindText(item->file_id);
		if (text) text->expanded = expanded;
	}
}

static void Config_ActivateItem(config_layout_item_t *item)
{
	if (!item) return;
	if (item->kind == CONFIG_ITEM_SECTION)
		Config_SetExpanded(item, !config_menu.section_open[item->auxiliary]);
	else if (item->kind == CONFIG_ITEM_CLASS)
		Config_SetExpanded(item, !config_menu.class_open[item->auxiliary]);
	else if (item->kind == CONFIG_ITEM_MISC) {
		config_text_draft_t *text = Config_FindText(item->file_id);
		Config_SetExpanded(item, !(text && text->expanded));
	}
	else if (item->kind == CONFIG_ITEM_SETTING) {
		config_setting_draft_t *draft = Config_SettingAt(item->file_id, item->data_index);
		if (draft->definition->widget_type == CFG_WIDGET_CHECKBOX ||
			draft->definition->widget_type == CFG_WIDGET_SELECT) Config_AdjustSetting(draft, 1);
		else Config_BeginEdit(draft);
	}
	else if (item->kind == CONFIG_ITEM_BIND)
		CKeyCapture_Begin(&Config_BindAt(item->file_id, item->data_index)->control);
	else if (item->kind == CONFIG_ITEM_TEXTAREA)
		config_menu.textarea_editing = true;
	Config_BuildLayout();
}

static void Config_MoveSection(int direction)
{
	int i = config_menu.cursor;
	if (!config_menu.layout_count) return;
	do {
		i = (i + config_menu.layout_count + direction) % config_menu.layout_count;
		if (config_menu.layout[i].kind == CONFIG_ITEM_SECTION) {
			config_menu.cursor = i;
			return;
		}
	} while (i != config_menu.cursor);
}

void Menu_Config_Key(int key, wchar unichar)
{
	config_layout_item_t *item;
	if (!config_menu.loaded) {
		if (key == K_ENTER) Config_LoadSession();
		else if (key == K_ESCAPE || key == K_MOUSE2) M_LeaveMenu(m_main);
		return;
	}
	item = Config_SelectedItem();
	if (!item) return;

	if (config_menu.editing && item->kind == CONFIG_ITEM_SETTING) {
		config_setting_draft_t *draft = Config_SettingAt(item->file_id, item->data_index);
		if (key == K_ESCAPE) config_menu.editing = false;
		else if (key == K_ENTER) {
			strlcpy(draft->value, config_menu.editbox.text, sizeof(draft->value));
			config_menu.editing = false;
		}
		else CEditBox_Key(&config_menu.editbox, key, unichar);
		return;
	}
	if (config_menu.textarea_editing && item->kind == CONFIG_ITEM_TEXTAREA) {
		config_text_draft_t *text = Config_FindText(item->file_id);
		if ((key == 's' || key == 'S') &&
			(keydown[K_CTRL] || keydown[K_LCTRL] || keydown[K_RCTRL])) Config_SaveSession();
		else if (key == K_ESCAPE || key == K_MOUSE2) config_menu.textarea_editing = false;
		else if (text) CTextArea_Key(&text->area, key, unichar);
		return;
	}
	if (item->kind == CONFIG_ITEM_BIND) {
		config_bind_draft_t *bind = Config_BindAt(item->file_id, item->data_index);
		if (bind->control.capturing) {
			CKeyCapture_Key(&bind->control, key);
			return;
		}
	}

	switch (key) {
		case K_ESCAPE: case K_MOUSE2: M_LeaveMenu(m_main); return;
		case 's': case 'S':
			if (keydown[K_CTRL] || keydown[K_LCTRL] || keydown[K_RCTRL]) Config_SaveSession();
			return;
		case 'r': case 'R':
			if (keydown[K_CTRL] || keydown[K_LCTRL] || keydown[K_RCTRL]) Config_LoadSession();
			return;
		case K_TAB:
			Config_MoveSection(keydown[K_SHIFT] ? -1 : 1);
			S_LocalSound("misc/menu1.wav");
			break;
		case K_UPARROW: case K_MWHEELUP:
			config_menu.cursor = (config_menu.cursor + config_menu.layout_count - 1) % config_menu.layout_count;
			S_LocalSound("misc/menu1.wav");
			break;
		case K_DOWNARROW: case K_MWHEELDOWN:
			config_menu.cursor = (config_menu.cursor + 1) % config_menu.layout_count;
			S_LocalSound("misc/menu1.wav");
			break;
		case K_HOME: config_menu.cursor = 0; break;
		case K_END: config_menu.cursor = config_menu.layout_count - 1; break;
		case K_PGUP: config_menu.cursor = max(0, config_menu.cursor - 8); break;
		case K_PGDN: config_menu.cursor = min(config_menu.layout_count - 1, config_menu.cursor + 8); break;
		case K_LEFTARROW: case K_RIGHTARROW:
			if (item->kind == CONFIG_ITEM_SECTION || item->kind == CONFIG_ITEM_CLASS || item->kind == CONFIG_ITEM_MISC)
				Config_SetExpanded(item, key == K_RIGHTARROW);
			else if (item->kind == CONFIG_ITEM_SETTING) {
				config_setting_draft_t *draft = Config_SettingAt(item->file_id, item->data_index);
				if (draft->definition->widget_type == CFG_WIDGET_CHECKBOX ||
					draft->definition->widget_type == CFG_WIDGET_SELECT)
					Config_AdjustSetting(draft, key == K_RIGHTARROW ? 1 : -1);
			}
			Config_BuildLayout();
			break;
		case K_BACKSPACE: case K_DEL:
			if (item->kind == CONFIG_ITEM_BIND)
				CKeyCapture_Key(&Config_BindAt(item->file_id, item->data_index)->control, key);
			break;
		case K_ENTER: case K_MOUSE1:
			m_entersound = true;
			Config_ActivateItem(item);
			break;
	}
	Config_KeepCursorVisible();
}

qbool Menu_Config_IsCapturingKey(void)
{
	config_layout_item_t *item;
	if (config_menu.editing || config_menu.textarea_editing) return true;
	item = Config_SelectedItem();
	if (item && item->kind == CONFIG_ITEM_BIND) {
		config_bind_draft_t *draft = Config_BindAt(item->file_id, item->data_index);
		return draft && draft->control.capturing;
	}
	return false;
}

qbool Menu_Config_Mouse_Event(const mouse_state_t *ms)
{
	int i;
	if (!config_menu.loaded) {
		if (ms->button_up == 1) Menu_Config_Key(K_ENTER, 0);
		else if (ms->button_up == 2) Menu_Config_Key(K_MOUSE2, 0);
		return true;
	}
	if (ms->button_up == 2) {
		Menu_Config_Key(K_MOUSE2, 0);
		return true;
	}
	if (ms->button_up == 4) { Menu_Config_Key(K_MWHEELUP, 0); return true; }
	if (ms->button_up == 5) { Menu_Config_Key(K_MWHEELDOWN, 0); return true; }
	Config_BuildLayout();
	if (config_menu.textarea_editing) {
		config_layout_item_t *selected = Config_SelectedItem();
		if (selected && selected->kind == CONFIG_ITEM_TEXTAREA) {
			config_text_draft_t *draft = Config_FindText(selected->file_id);
			int item_y = config_menu.viewport_top + selected->content_y - config_menu.scroll;
			if (draft) {
				float character_width = CTextArea_CharacterWidth(&draft->area);
				int area_x = Config_ValueColumnX() + 8;
				int area_y = item_y + 10;
				int track_width = max(3, (int)(character_width * 0.75f));
				int track_x = area_x + (int)(draft->area.width * character_width) - track_width;
				int track_height = draft->area.height * 8;
				if (ms->x >= track_x - 2 && ms->x <= track_x + track_width + 2 &&
					ms->y >= area_y && ms->y <= area_y + track_height &&
					(ms->button_down == 1 || ms->button_up == 1 || ms->buttons[1])) {
					CTextArea_SetScrollFraction(&draft->area,
						(float)(ms->y - area_y) / max(1, track_height));
					return true;
				}
			}
		}
	}
	for (i = 0; i < config_menu.layout_count; ++i) {
		config_layout_item_t *item = &config_menu.layout[i];
		int y = config_menu.viewport_top + item->content_y - config_menu.scroll;
		if (ms->y >= y && ms->y < y + item->height &&
			ms->y >= config_menu.viewport_top && ms->y < config_menu.viewport_bottom) {
			config_menu.cursor = i;
			if (ms->button_up == 1) Menu_Config_Key(K_MOUSE1, 0);
			return true;
		}
	}
	return false;
}

void Menu_Config_Enter(void)
{
	if (!config_menu.loaded) Config_LoadSession();
	config_menu.cursor = config_menu.scroll = 0;
	config_menu.editing = config_menu.textarea_editing = false;
	M_EnterMenu(m_config);
}

void Menu_Config_Init(void)
{
	memset(&config_menu, 0, sizeof(config_menu));
}

void Menu_Config_Shutdown(void)
{
	Config_FreeSession();
}
