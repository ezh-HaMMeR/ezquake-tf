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

#define CONFIG_PATH_PART "qw/config_editor"
#define CONFIG_VALUE_SIZE 256

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
} config_menu_t;

static config_menu_t config_menu;

static const char *config_sections[] = {
	"Main Settings", "Binds", "Class Settings", "HUD Settings"
};

static const char *config_classes[] = {
	"Scout", "Sniper", "Soldier", "Demoman", "Medic",
	"HWGuy", "Pyro", "Spy", "Engineer"
};

static const char *config_class_ids[] = {
	"class_scout", "class_sniper", "class_soldier", "class_demoman", "class_medic",
	"class_hwguy", "class_pyro", "class_spy", "class_engineer"
};

static void Config_DrawUTF8(int x, int y, const char *text, qbool active, int max_chars)
{
	wchar wide[512];
	int input = 0, output = 0, length;

	if (!text) text = "";
	length = (int)strlen(text);
	while (input < length && output < (int)(sizeof(wide) / sizeof(wide[0])) - 1 &&
		(max_chars <= 0 || output < max_chars)) {
		wide[output++] = TextEncodingDecodeUTF8((char *)text, &input);
		++input;
	}
	wide[output] = 0;
	Draw_ConsoleString(x, y, wide, NULL, 0, active, 1, false);
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

static void Config_DrawHelpBox(const char *text)
{
	int height = 40;
	int y = vid.height - OPTPADDING - height;
	UI_DrawBox(OPTPADDING, y, vid.width - OPTPADDING * 2, height);
	Config_DrawUTF8(OPTPADDING + LETTERWIDTH, y + LETTERHEIGHT, text, false,
		max(20, vid.width / LETTERWIDTH - 4));
	UI_Print(OPTPADDING + LETTERWIDTH, y + LETTERHEIGHT * 3,
		"Enter: edit/open   Tab: next section   Ctrl+R: reload   Esc: back", false);
}

static void Config_DrawLoadError(void)
{
	UI_Print(OPTPADDING, OPTPADDING, "Config", true);
	UI_DrawBox(OPTPADDING, 24, vid.width - OPTPADDING * 2, 64);
	UI_Print(OPTPADDING + 8, 32, "Unable to load config files:", true);
	UI_Print(OPTPADDING + 8, 48, config_menu.error, false);
	UI_Print(OPTPADDING + 8, 72, "Enter: retry   Esc: back", false);
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
	if (include_settings) for (i = 0; i < Config_SettingCount(file_id); ++i)
		Config_AddLayout(CONFIG_ITEM_SETTING, file_id, i, 0, 10);
	if (include_binds) for (i = 0; i < Config_BindCount(file_id); ++i)
		Config_AddLayout(CONFIG_ITEM_BIND, file_id, i, 0, 10);
	Config_AddLayout(CONFIG_ITEM_MISC, file_id, 0, 0, 12);
	if (text && text->expanded) Config_AddLayout(CONFIG_ITEM_TEXTAREA, file_id, 0, 0, 72);
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
			Config_AddLayout(CONFIG_ITEM_TEXTAREA, "hud", 0, 0, 72);
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

static void Config_DrawSettingValue(config_setting_draft_t *draft, int x, int y, qbool active)
{
	const cfg_setting_definition_t *definition = draft->definition;
	const char *value = draft->value;
	char field[CONFIG_VALUE_SIZE + 8];

	if (active && config_menu.editing) {
		CEditBox_Draw(&config_menu.editbox, x, y, true);
		return;
	}
	if (definition->widget_type == CFG_WIDGET_CHECKBOX) {
		value = !strcmp(draft->value, definition->checked_value) ? "Да" : "Нет";
	}
	else if (definition->widget_type == CFG_WIDGET_SELECT) {
		size_t i;
		for (i = 0; i < definition->option_count; ++i)
			if (!strcmp(value, definition->options[i].value)) value = definition->options[i].label;
		Config_DrawUTF8(x, y, value, active, 40);
		return;
	}
	snprintf(field, sizeof(field), "[ %s ]", value);
	Config_DrawUTF8(x, y, field, active, 40);
}

static void Config_DrawTextArea(const config_layout_item_t *item, int x, int y, int available_width, qbool active)
{
	config_text_draft_t *draft = Config_FindText(item->file_id);
	int box_width = min(max(400, available_width - 32), 960);
	int box_height = item->height - 8;
	if (!draft) return;
	UI_DrawBox(x + 16, y + 2, box_width, box_height);
	draft->area.width = max(24, box_width / 8 - 2);
	draft->area.height = max(3, box_height / 8 - 2);
	CTextArea_Draw(&draft->area, x + 24, y + 10, active && config_menu.textarea_editing);
	if (!draft->area.length)
		Config_DrawUTF8(x + 24, y + 10, "(нет строк для отображения)", false, 40);
}

static void Config_DrawLayoutItem(const config_layout_item_t *item, int index, int y)
{
	qbool active = index == config_menu.cursor;
	int left = OPTPADDING;
	int width = vid.width - OPTPADDING * 2 - 12;
	int indent = 0;
	char label[96];

	if (item->kind == CONFIG_ITEM_TEXTAREA) {
		Config_DrawTextArea(item, left, y, width, active);
		return;
	}
	if (item->kind == CONFIG_ITEM_CLASS) indent = 12;
	else if (item->kind != CONFIG_ITEM_SECTION) indent = 28;
	if (active) UI_DrawGrayBox(left + indent, y, width - indent, item->height - 2);

	if (item->kind == CONFIG_ITEM_SECTION) {
		snprintf(label, sizeof(label), "[%c] %s", config_menu.section_open[item->auxiliary] ? '-' : '+',
			config_sections[item->auxiliary]);
		UI_Print(left + 4, y + 3, label, active);
	}
	else if (item->kind == CONFIG_ITEM_CLASS) {
		snprintf(label, sizeof(label), "  [%c] %s", config_menu.class_open[item->auxiliary] ? '-' : '+',
			config_classes[item->auxiliary]);
		UI_Print(left + indent, y + 2, label, active);
	}
	else if (item->kind == CONFIG_ITEM_MISC) {
		config_text_draft_t *text = Config_FindText(item->file_id);
		snprintf(label, sizeof(label), "    [%c] ", text && text->expanded ? '-' : '+');
		UI_Print(left + indent, y + 2, label, active);
		Config_DrawUTF8(left + indent + 8 * LETTERWIDTH, y + 2,
			"Остальные настройки...", active, 40);
	}
	else {
		int value_x = left + width / 2 + LETTERWIDTH * 2;
		int label_chars = max(12, (value_x - left - indent) / LETTERWIDTH - 3);
		if (item->kind == CONFIG_ITEM_SETTING) {
			config_setting_draft_t *draft = Config_SettingAt(item->file_id, item->data_index);
			int label_length = min(label_chars, Config_UTF8Length(draft->definition->label));
			Config_DrawUTF8(value_x - label_length * LETTERWIDTH - LETTERWIDTH * 2, y,
				draft->definition->label, active, label_chars);
			Config_DrawSettingValue(draft, value_x, y, active);
		}
		else {
			config_bind_draft_t *draft = Config_BindAt(item->file_id, item->data_index);
			int label_length = min(label_chars, Config_UTF8Length(draft->definition->label));
			Config_DrawUTF8(value_x - label_length * LETTERWIDTH - LETTERWIDTH * 2, y,
				draft->definition->label, active, label_chars);
			CKeyCapture_Draw(&draft->control, value_x, y, 22, active);
		}
	}
}

static const char *Config_SelectedHelp(void)
{
	config_layout_item_t *item = config_menu.layout_count ? &config_menu.layout[config_menu.cursor] : NULL;
	if (!item) return "";
	if (item->kind == CONFIG_ITEM_SETTING)
		return Config_SettingAt(item->file_id, item->data_index)->definition->description;
	if (item->kind == CONFIG_ITEM_BIND)
		return Config_BindAt(item->file_id, item->data_index)->definition->description;
	if (item->kind == CONFIG_ITEM_MISC || (item->kind == CONFIG_ITEM_TEXTAREA && strcmp(item->file_id, "hud")))
		return "Неизвестные строки сохраняют исходный файл, положение и порядок.";
	if (item->kind == CONFIG_ITEM_TEXTAREA)
		return "Полный hud.cfg. Enter включает редактирование текста, Esc возвращает управление странице.";
	if (item->kind == CONFIG_ITEM_CLASS)
		return "Настройки, бинды и остальные строки выбранного классового CFG.";
	return "Раздел можно свернуть или развернуть клавишей Enter.";
}

void Menu_Config_Draw(void)
{
	int i;
	M_Unscale_Menu();
	if (!config_menu.loaded) {
		Config_DrawLoadError();
		return;
	}
	config_menu.viewport_top = OPTPADDING + LETTERHEIGHT * 2;
	config_menu.viewport_bottom = vid.height - OPTPADDING - 44;
	Config_BuildLayout();
	Config_KeepCursorVisible();
	UI_Print(OPTPADDING, OPTPADDING, "Config", true);
	UI_Print(OPTPADDING, OPTPADDING + LETTERHEIGHT, "------------------------------------------------------------", false);
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
		if (key == K_ESCAPE || key == K_MOUSE2) config_menu.textarea_editing = false;
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
