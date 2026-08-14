#include "quakedef.h"
#include "menu.h"
#include "menu_config.h"
#include "cfg_editor_dictionary.h"
#include "Ctrl_Checkbox.h"
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
#define CONFIG_VISIBLE_MIN 8

typedef enum config_screen_e {
	CONFIG_SCREEN_ROOT,
	CONFIG_SCREEN_MAIN,
	CONFIG_SCREEN_BINDS,
	CONFIG_SCREEN_CLASSES,
	CONFIG_SCREEN_CLASS,
	CONFIG_SCREEN_HUD,
	CONFIG_SCREEN_MISC
} config_screen_t;

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
	config_screen_t screen;
	int cursor;
	int scroll;
	int selected_class;
	char misc_file_id[64];
	qbool editing;
	CEditBox editbox;
	int hovered_tab;
	int tab_left[4];
	int tab_right[4];
	int rows_top;
	int rows_bottom;
} config_menu_t;

static config_menu_t config_menu;

static const char *config_tabs[] = {
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
	config_menu.screen = CONFIG_SCREEN_ROOT;
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

static void Config_OpenScreen(config_screen_t screen)
{
	config_menu.screen = screen;
	config_menu.cursor = 0;
	config_menu.scroll = 0;
	config_menu.editing = false;
}

static int Config_VisibleRows(void)
{
	return max(CONFIG_VISIBLE_MIN, (vid.height - 92) / 10);
}

static void Config_KeepCursorVisible(int item_count)
{
	int visible = Config_VisibleRows();
	if (item_count <= 0) config_menu.cursor = 0;
	else config_menu.cursor = bound(0, config_menu.cursor, item_count - 1);
	if (config_menu.cursor < config_menu.scroll) config_menu.scroll = config_menu.cursor;
	if (config_menu.cursor >= config_menu.scroll + visible) config_menu.scroll = config_menu.cursor - visible + 1;
}

static int Config_TabForScreen(void)
{
	switch (config_menu.screen) {
		case CONFIG_SCREEN_BINDS: return 1;
		case CONFIG_SCREEN_CLASSES:
		case CONFIG_SCREEN_CLASS: return 2;
		case CONFIG_SCREEN_HUD: return 3;
		default: return 0;
	}
}

static int Config_DrawTabs(void)
{
	char line[1024];
	int i, x = OPTPADDING, active_tab = Config_TabForScreen();
	for (i = 0; i < 4; ++i) {
		char label[40];
		config_menu.tab_left[i] = x;
		snprintf(label, sizeof(label), i == active_tab ? "\x10%s\x11" : " %s ", config_tabs[i]);
		UI_Print(x, OPTPADDING, label, i == active_tab || i == config_menu.hovered_tab);
		x += ((int)strlen(config_tabs[i]) + 2) * LETTERWIDTH;
		config_menu.tab_right[i] = x;
	}
	memset(line, '\x1e', min((int)sizeof(line) - 1, vid.width / LETTERWIDTH));
	line[min((int)sizeof(line) - 1, vid.width / LETTERWIDTH)] = '\0';
	line[0] = '\x1d';
	line[max(1, min((int)sizeof(line) - 2, vid.width / LETTERWIDTH - 1))] = '\x1f';
	UI_Print(OPTPADDING, OPTPADDING + LETTERHEIGHT, line, false);
	return OPTPADDING + LETTERHEIGHT * 2;
}

static void Config_DrawHelpBox(const char *text)
{
	int height = 40;
	int y = vid.height - OPTPADDING - height;
	UI_DrawBox(OPTPADDING, y, vid.width - OPTPADDING * 2, height);
	Config_DrawUTF8(OPTPADDING + LETTERWIDTH, y + LETTERHEIGHT, text, false,
		max(20, vid.width / LETTERWIDTH - 4));
	UI_Print(OPTPADDING + LETTERWIDTH, y + LETTERHEIGHT * 3,
		"Tab/PgUp/PgDn: section   Ctrl+R: discard draft/reload   Esc: back", false);
}

static void Config_DrawLoadError(void)
{
	UI_Print(OPTPADDING, OPTPADDING, "Config", true);
	UI_DrawBox(OPTPADDING, 24, vid.width - OPTPADDING * 2, 64);
	UI_Print(OPTPADDING + 8, 32, "Unable to load config files:", true);
	UI_Print(OPTPADDING + 8, 48, config_menu.error, false);
	UI_Print(OPTPADDING + 8, 72, "Enter: retry   Esc: back", false);
}

static void Config_DrawSettingValue(config_setting_draft_t *draft, int x, int y, qbool active)
{
	const cfg_setting_definition_t *definition = draft->definition;
	if (active && config_menu.editing) {
		CEditBox_Draw(&config_menu.editbox, x, y, true);
	}
	else if (definition->widget_type == CFG_WIDGET_CHECKBOX) {
		checkbox_control_t checkbox;
		CCheckbox_Init(&checkbox, !strcmp(draft->value, definition->checked_value),
			definition->checked_value, definition->unchecked_value);
		CCheckbox_Draw(&checkbox, x, y, active);
	}
	else if (definition->widget_type == CFG_WIDGET_SLIDER) {
		double minimum = definition->has_minimum ? definition->minimum : 0;
		double maximum = definition->has_maximum ? definition->maximum : 1;
		double range = maximum > minimum ? (atof(draft->value) - minimum) / (maximum - minimum) : 0;
		int value_x = UI_DrawSlider(x, y, range);
		Draw_String(value_x + LETTERWIDTH, y, draft->value);
	}
	else if (definition->widget_type == CFG_WIDGET_SELECT) {
		const char *value = draft->value;
		size_t i;
		for (i = 0; i < definition->option_count; ++i)
			if (!strcmp(value, definition->options[i].value)) value = definition->options[i].label;
		Config_DrawUTF8(x, y, value, active, 24);
	}
	else {
		Draw_String(x, y, draft->value);
	}
}

static void Config_DrawList(const char *file_id, qbool include_settings, qbool include_binds, const char *title)
{
	int setting_count = include_settings ? Config_SettingCount(file_id) : 0;
	int bind_count = include_binds ? Config_BindCount(file_id) : 0;
	int item_count = setting_count + bind_count + 1;
	int visible = Config_VisibleRows(), row, label_chars;
	int top = Config_DrawTabs();
	int help_height = 44;
	int width = vid.width - OPTPADDING * 2 - 12, left = OPTPADDING;
	int value_x = left + width / 2 + LETTERWIDTH * 2;
	Config_KeepCursorVisible(item_count);
	(void)title;
	label_chars = max(12, (value_x - left) / 8 - 3);
	visible = min(visible, max(1, (vid.height - top - help_height) / 10));
	config_menu.rows_top = top;
	config_menu.rows_bottom = top + visible * 10;

	for (row = 0; row < visible && config_menu.scroll + row < item_count; ++row) {
		int index = config_menu.scroll + row;
		int y = top + row * 10;
		qbool active = index == config_menu.cursor;
		if (active) UI_DrawGrayBox(left, y, width, 9);
		if (index < setting_count) {
			config_setting_draft_t *draft = Config_SettingAt(file_id, index);
			int label_length = min(label_chars, Config_UTF8Length(draft->definition->label));
			Config_DrawUTF8(left + width / 2 - label_length * LETTERWIDTH, y,
				draft->definition->label, active, label_chars);
			Config_DrawSettingValue(draft, value_x, y, active);
		}
		else if (index < setting_count + bind_count) {
			config_bind_draft_t *draft = Config_BindAt(file_id, index - setting_count);
			int label_length = min(label_chars, Config_UTF8Length(draft->definition->label));
			Config_DrawUTF8(left + width / 2 - label_length * LETTERWIDTH, y,
				draft->definition->label, active, label_chars);
			CKeyCapture_Draw(&draft->control, value_x, y, 22, active);
		}
		else {
			UI_Print_Center(left, y, width, "Misc (unknown lines)...", active);
		}
	}

	if (config_menu.cursor < setting_count) {
		config_setting_draft_t *draft = Config_SettingAt(file_id, config_menu.cursor);
		Config_DrawHelpBox(draft->definition->description);
	}
	else if (config_menu.cursor < setting_count + bind_count) {
		config_bind_draft_t *draft = Config_BindAt(file_id, config_menu.cursor - setting_count);
		Config_DrawHelpBox(draft->definition->description);
	}
	else Config_DrawHelpBox("Unknown lines retain their original source file and line references.");
}

static void Config_DrawClasses(void)
{
	int i, top = Config_DrawTabs(), width = vid.width - OPTPADDING * 2 - 12;
	config_menu.rows_top = top;
	config_menu.rows_bottom = top + 9 * 12;
	for (i = 0; i < 9; ++i) {
		int y = top + i * 12;
		if (i == config_menu.cursor) UI_DrawGrayBox(OPTPADDING, y, width, 9);
		UI_Print_Center(OPTPADDING, y, width, config_classes[i], i == config_menu.cursor);
	}
	Config_DrawHelpBox("Choose a class. Its settings, binds and unknown CFG lines are edited together.");
}

static void Config_DrawText(const char *file_id, const char *title)
{
	config_text_draft_t *draft = Config_FindText(file_id);
	int top = Config_DrawTabs();
	int width = vid.width - OPTPADDING * 2 - 12, left = OPTPADDING;
	int box_height = vid.height - top - 48;
	int chars = max(24, width / 8 - 4), rows = max(8, box_height / 8 - 2);
	(void)title;
	if (!draft) {
		UI_Print(left, top, "No text draft is available for this file", true);
		return;
	}
	UI_DrawBox(left, top, width, box_height);
	draft->area.width = chars;
	draft->area.height = rows;
	CTextArea_Draw(&draft->area, left + LETTERWIDTH, top + LETTERHEIGHT, true);
	Config_DrawHelpBox(draft->area.show_source_gutter ?
		"Misc draft. The left gutter shows the original CFG file and line." :
		"HUD draft. The complete hud.cfg is shown without dictionary interpretation.");
}

void Menu_Config_Draw(void)
{
	char title[128];
	M_Unscale_Menu();
	if (!config_menu.loaded) {
		Config_DrawLoadError();
		return;
	}
	switch (config_menu.screen) {
		case CONFIG_SCREEN_ROOT: Config_OpenScreen(CONFIG_SCREEN_MAIN); Config_DrawList("main", true, false, "Config / Main Settings"); break;
		case CONFIG_SCREEN_MAIN: Config_DrawList("main", true, false, "Config / Main Settings"); break;
		case CONFIG_SCREEN_BINDS: Config_DrawList("binds", false, true, "Config / Binds"); break;
		case CONFIG_SCREEN_CLASSES: Config_DrawClasses(); break;
		case CONFIG_SCREEN_CLASS:
			snprintf(title, sizeof(title), "Config / Class Settings / %s", config_classes[config_menu.selected_class]);
			Config_DrawList(config_class_ids[config_menu.selected_class], true, true, title);
			break;
		case CONFIG_SCREEN_HUD: Config_DrawText("hud", "Config / HUD Settings / hud.cfg"); break;
		case CONFIG_SCREEN_MISC:
			snprintf(title, sizeof(title), "Config / Misc / %s", config_menu.misc_file_id);
			Config_DrawText(config_menu.misc_file_id, title);
			break;
		default: break;
	}
}

static int Config_CurrentCount(const char **file_id, int *setting_count, int *bind_count)
{
	*setting_count = *bind_count = 0;
	if (config_menu.screen == CONFIG_SCREEN_MAIN) {
		*file_id = "main"; *setting_count = Config_SettingCount(*file_id);
	}
	else if (config_menu.screen == CONFIG_SCREEN_BINDS) {
		*file_id = "binds"; *bind_count = Config_BindCount(*file_id);
	}
	else if (config_menu.screen == CONFIG_SCREEN_CLASS) {
		*file_id = config_class_ids[config_menu.selected_class];
		*setting_count = Config_SettingCount(*file_id);
		*bind_count = Config_BindCount(*file_id);
	}
	return *setting_count + *bind_count + 1;
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
	else if (definition->widget_type == CFG_WIDGET_SLIDER || definition->widget_type == CFG_WIDGET_NUMBER) {
		double value = atof(draft->value);
		double step = definition->has_step ? definition->step : 1;
		value += step * direction;
		if (definition->has_minimum) value = max(value, definition->minimum);
		if (definition->has_maximum) value = min(value, definition->maximum);
		if (step < 0.01) snprintf(draft->value, sizeof(draft->value), "%.3f", value);
		else if (step < 1) snprintf(draft->value, sizeof(draft->value), "%.2f", value);
		else snprintf(draft->value, sizeof(draft->value), "%.0f", value);
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

static void Config_Back(void)
{
	if (config_menu.screen == CONFIG_SCREEN_CLASS) Config_OpenScreen(CONFIG_SCREEN_CLASSES);
	else if (config_menu.screen == CONFIG_SCREEN_MISC) {
		if (!strcmp(config_menu.misc_file_id, "main")) Config_OpenScreen(CONFIG_SCREEN_MAIN);
		else if (!strcmp(config_menu.misc_file_id, "binds")) Config_OpenScreen(CONFIG_SCREEN_BINDS);
		else Config_OpenScreen(CONFIG_SCREEN_CLASS);
	}
	else M_LeaveMenu(m_main);
}

static void Config_SwitchTab(int direction)
{
	int tab = (Config_TabForScreen() + 4 + direction) % 4;
	switch (tab) {
		case 0: Config_OpenScreen(CONFIG_SCREEN_MAIN); break;
		case 1: Config_OpenScreen(CONFIG_SCREEN_BINDS); break;
		case 2: Config_OpenScreen(CONFIG_SCREEN_CLASSES); break;
		case 3: Config_OpenScreen(CONFIG_SCREEN_HUD); break;
	}
}

void Menu_Config_Key(int key, wchar unichar)
{
	const char *file_id = NULL;
	int setting_count = 0, bind_count = 0, item_count;

	if (!config_menu.loaded) {
		if (key == K_ENTER) Config_LoadSession();
		else if (key == K_ESCAPE || key == K_MOUSE2) M_LeaveMenu(m_main);
		return;
	}

	if (config_menu.screen == CONFIG_SCREEN_HUD || config_menu.screen == CONFIG_SCREEN_MISC) {
		config_text_draft_t *draft = Config_FindText(config_menu.screen == CONFIG_SCREEN_HUD ? "hud" : config_menu.misc_file_id);
		if (key == K_ESCAPE || key == K_MOUSE2) Config_Back();
		else if (config_menu.screen == CONFIG_SCREEN_HUD && key == K_TAB)
			Config_SwitchTab(keydown[K_SHIFT] ? -1 : 1);
		else if (config_menu.screen == CONFIG_SCREEN_HUD && key == K_PGUP) Config_SwitchTab(-1);
		else if (config_menu.screen == CONFIG_SCREEN_HUD && key == K_PGDN) Config_SwitchTab(1);
		else if (draft) CTextArea_Key(&draft->area, key, unichar);
		return;
	}

	if (config_menu.screen == CONFIG_SCREEN_CLASSES) item_count = 9;
	else item_count = Config_CurrentCount(&file_id, &setting_count, &bind_count);

	if (file_id && config_menu.cursor < setting_count) {
		config_setting_draft_t *draft = Config_SettingAt(file_id, config_menu.cursor);
		if (config_menu.editing) {
			if (key == K_ESCAPE) config_menu.editing = false;
			else if (key == K_ENTER) {
				strlcpy(draft->value, config_menu.editbox.text, sizeof(draft->value));
				config_menu.editing = false;
			}
			else CEditBox_Key(&config_menu.editbox, key, unichar);
			return;
		}
	}
	if (file_id && config_menu.cursor >= setting_count && config_menu.cursor < setting_count + bind_count) {
		config_bind_draft_t *draft = Config_BindAt(file_id, config_menu.cursor - setting_count);
		if (draft->control.capturing) {
			CKeyCapture_Key(&draft->control, key);
			return;
		}
	}

	switch (key) {
		case K_ESCAPE: case K_MOUSE2: Config_Back(); return;
		case K_TAB:
			Config_SwitchTab(keydown[K_SHIFT] ? -1 : 1);
			S_LocalSound("misc/menu1.wav");
			return;
		case K_PGUP: Config_SwitchTab(-1); S_LocalSound("misc/menu1.wav"); return;
		case K_PGDN: Config_SwitchTab(1); S_LocalSound("misc/menu1.wav"); return;
		case 'r': case 'R':
			if (keydown[K_CTRL] || keydown[K_LCTRL] || keydown[K_RCTRL]) Config_LoadSession();
			return;
		case K_UPARROW: case K_MWHEELUP:
			if (--config_menu.cursor < 0) config_menu.cursor = item_count - 1;
			S_LocalSound("misc/menu1.wav"); break;
		case K_DOWNARROW: case K_MWHEELDOWN:
			if (++config_menu.cursor >= item_count) config_menu.cursor = 0;
			S_LocalSound("misc/menu1.wav"); break;
		case K_HOME: config_menu.cursor = 0; break;
		case K_END: config_menu.cursor = item_count - 1; break;
		case K_LEFTARROW: case K_RIGHTARROW:
			if (file_id && config_menu.cursor < setting_count)
				Config_AdjustSetting(Config_SettingAt(file_id, config_menu.cursor), key == K_RIGHTARROW ? 1 : -1);
			break;
		case K_BACKSPACE: case K_DEL:
			if (file_id && config_menu.cursor >= setting_count && config_menu.cursor < setting_count + bind_count)
				CKeyCapture_Key(&Config_BindAt(file_id, config_menu.cursor - setting_count)->control, key);
			break;
		case K_ENTER: case K_MOUSE1:
			m_entersound = true;
			if (config_menu.screen == CONFIG_SCREEN_CLASSES) {
				config_menu.selected_class = config_menu.cursor;
				Config_OpenScreen(CONFIG_SCREEN_CLASS);
			}
			else if (file_id && config_menu.cursor < setting_count) {
				config_setting_draft_t *draft = Config_SettingAt(file_id, config_menu.cursor);
				if (draft->definition->widget_type == CFG_WIDGET_CHECKBOX || draft->definition->widget_type == CFG_WIDGET_SELECT)
					Config_AdjustSetting(draft, 1);
				else Config_BeginEdit(draft);
			}
			else if (file_id && config_menu.cursor < setting_count + bind_count) {
				CKeyCapture_Begin(&Config_BindAt(file_id, config_menu.cursor - setting_count)->control);
			}
			else if (file_id) {
				strlcpy(config_menu.misc_file_id, file_id, sizeof(config_menu.misc_file_id));
				Config_OpenScreen(CONFIG_SCREEN_MISC);
			}
			break;
	}
	Config_KeepCursorVisible(item_count);
}

qbool Menu_Config_IsCapturingKey(void)
{
	const char *file_id = NULL;
	int setting_count = 0, bind_count = 0;
	if (config_menu.editing) return true;
	if (config_menu.screen != CONFIG_SCREEN_MAIN && config_menu.screen != CONFIG_SCREEN_BINDS && config_menu.screen != CONFIG_SCREEN_CLASS)
		return false;
	Config_CurrentCount(&file_id, &setting_count, &bind_count);
	if (file_id && config_menu.cursor >= setting_count && config_menu.cursor < setting_count + bind_count) {
		config_bind_draft_t *draft = Config_BindAt(file_id, config_menu.cursor - setting_count);
		return draft && draft->control.capturing;
	}
	return false;
}

qbool Menu_Config_Mouse_Event(const mouse_state_t *ms)
{
	int i;
	config_menu.hovered_tab = -1;
	if (!config_menu.loaded) {
		if (ms->button_up == 1) Menu_Config_Key(K_ENTER, 0);
		else if (ms->button_up == 2) Menu_Config_Key(K_MOUSE2, 0);
		return true;
	}
	for (i = 0; i < 4; ++i) {
		if (ms->y >= OPTPADDING && ms->y < OPTPADDING + LETTERHEIGHT &&
			ms->x >= config_menu.tab_left[i] && ms->x < config_menu.tab_right[i]) {
			config_menu.hovered_tab = i;
			if (ms->button_up == 1) {
				int direction = i - Config_TabForScreen();
				if (direction) Config_SwitchTab(direction);
				return true;
			}
		}
	}
	if (ms->button_up == 2) {
		Menu_Config_Key(K_MOUSE2, 0);
		return true;
	}
	if ((config_menu.screen == CONFIG_SCREEN_MAIN || config_menu.screen == CONFIG_SCREEN_BINDS ||
		config_menu.screen == CONFIG_SCREEN_CLASS) && ms->y >= config_menu.rows_top && ms->y < config_menu.rows_bottom) {
		const char *file_id = NULL;
		int setting_count, bind_count;
		int count = Config_CurrentCount(&file_id, &setting_count, &bind_count);
		config_menu.cursor = bound(0, config_menu.scroll + (ms->y - config_menu.rows_top) / 10, count - 1);
		if (ms->button_up == 1) Menu_Config_Key(K_MOUSE1, 0);
		return true;
	}
	if (config_menu.screen == CONFIG_SCREEN_CLASSES && ms->y >= config_menu.rows_top && ms->y < config_menu.rows_bottom) {
		config_menu.cursor = bound(0, (ms->y - config_menu.rows_top) / 12, 8);
		if (ms->button_up == 1) Menu_Config_Key(K_MOUSE1, 0);
		return true;
	}
	return config_menu.hovered_tab >= 0;
}

void Menu_Config_Enter(void)
{
	if (!config_menu.loaded) Config_LoadSession();
	if (config_menu.loaded) Config_OpenScreen(CONFIG_SCREEN_MAIN);
	M_EnterMenu(m_config);
}

void Menu_Config_Init(void)
{
	memset(&config_menu, 0, sizeof(config_menu));
	config_menu.hovered_tab = -1;
}

void Menu_Config_Shutdown(void)
{
	Config_FreeSession();
}
