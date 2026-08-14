/*
Copyright (C) 1996-2003 Id Software, Inc., A Nourai

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

$Id: cl_screen.c,v 1.156 2007-10-29 00:56:47 qqshka Exp $
*/

#include "quakedef.h"
#include "keys.h"
#include "menu.h"
#include "hud.h"
#include "cfg_editor_dictionary.h"
#include "textencoding.h"

#define MYBINDS_MAX_ROWS 128
#define MYBINDS_KEY_SIZE 64
#define MYBINDS_CHAR_WIDTH 8
#define MYBINDS_LINE_HEIGHT 8

typedef struct mybinds_row_s {
	char keys[MYBINDS_KEY_SIZE];
	const char *label;
} mybinds_row_t;

static cvar_t scr_centertime  = { "scr_centertime",  "2" };
static cvar_t scr_centershift = { "scr_centershift", "0" };
static cvar_t scr_centerspeed = { "scr_centerspeed", "8" };
static cvar_t showmybinds = { "showmybinds", "0" };

static cfg_editor_dictionary_t mybinds_dictionary;
static qbool mybinds_dictionary_attempted;
static qbool mybinds_dictionary_loaded;

/**************************** CENTER PRINTING ********************************/

static char	 scr_centerstring_lines[1024][41];

static float scr_centertime_start;   // for slow victory printing
static float scr_centertime_off;
static int   scr_center_lines;
static int   scr_erase_lines;
static int   scr_erase_center;

static const char *SCR_MyBindsClassScope(void)
{
	static const char *scopes[] = {
		NULL, "class:scout", "class:sniper", "class:soldier", "class:demoman",
		"class:medic", "class:hwguy", "class:pyro", "class:spy", "class:engineer"
	};
	const player_info_t *player;

	if (cls.state != ca_active || !cl.teamfortress || cl.spectator ||
		cl.playernum < 0 || cl.playernum >= MAX_CLIENTS || key_dest != key_game)
		return NULL;
	player = &cl.players[cl.playernum];
	if (player->spectator || player->playerclass < PC_SCOUT || player->playerclass > PC_ENGINEER ||
		(player->team_no <= 0 && !player->team[0]))
		return NULL;
	return scopes[player->playerclass];
}

static qbool SCR_MyBindsLoadDictionary(void)
{
	char settings[MAX_OSPATH], binds[MAX_OSPATH], error[512] = { 0 };

	if (mybinds_dictionary_attempted)
		return mybinds_dictionary_loaded;
	mybinds_dictionary_attempted = true;
	CFGDictionary_Init(&mybinds_dictionary);
	snprintf(settings, sizeof(settings), "%s/qw/config_editor/dict_settings.json", com_basedir);
	snprintf(binds, sizeof(binds), "%s/qw/config_editor/dict_binds.json", com_basedir);
	mybinds_dictionary_loaded = CFGDictionary_Load(&mybinds_dictionary,
		settings, binds, error, sizeof(error));
	if (!mybinds_dictionary_loaded)
		Com_Printf("Unable to load bind help dictionary: %s\n", error);
	return mybinds_dictionary_loaded;
}

static qbool SCR_MyBindsDefinitionApplies(const cfg_bind_definition_t *definition,
	const char *class_scope)
{
	size_t i;
	for (i = 0; i < definition->scope_count; ++i)
		if (!strcmp(definition->scopes[i], "global") || !strcmp(definition->scopes[i], class_scope))
			return true;
	return false;
}

static qbool SCR_MyBindsBindingMatches(const cfg_bind_definition_t *definition,
	const char *binding)
{
	if (!binding)
		return false;
	return definition->case_sensitive ? !strcmp(binding, definition->command) :
		!strcasecmp(binding, definition->command);
}

static int SCR_MyBindsCollect(mybinds_row_t rows[MYBINDS_MAX_ROWS], const char *class_scope)
{
	qbool english = false;
	cvar_t *language = Cvar_Find("menu_language");
	int count = 0;
	size_t definition_index;

	if (language)
		english = !strcasecmp(language->string, "English");
	for (definition_index = 0;
		definition_index < mybinds_dictionary.bind_count && count < MYBINDS_MAX_ROWS;
		++definition_index) {
		const cfg_bind_definition_t *definition = &mybinds_dictionary.binds[definition_index];
		int key;

		if (!SCR_MyBindsDefinitionApplies(definition, class_scope))
			continue;
		rows[count].keys[0] = '\0';
		for (key = 0; key < KEY_MAX_KEYS; ++key) {
			const char *key_name;
			if (!SCR_MyBindsBindingMatches(definition, keybindings[key]))
				continue;
			key_name = Key_KeynumToString(key);
			if (!key_name || !*key_name)
				continue;
			if (rows[count].keys[0])
				strlcat(rows[count].keys, " / ", sizeof(rows[count].keys));
			strlcat(rows[count].keys, key_name, sizeof(rows[count].keys));
		}
		if (!rows[count].keys[0])
			continue;
		rows[count].label = english ? definition->label_en : definition->label;
		++count;
	}
	return count;
}

static int SCR_MyBindsUTF8Length(const char *text)
{
	int input = 0, characters = 0, length = text ? (int)strlen(text) : 0;
	while (input < length) {
		TextEncodingDecodeUTF8((char *)text, &input);
		++input;
		++characters;
	}
	return characters;
}

static void SCR_MyBindsDrawUTF8(int x, int y, const char *text, int max_chars)
{
	wchar wide[256];
	int input = 0, output = 0, length = text ? (int)strlen(text) : 0;

	while (input < length && output < (int)(sizeof(wide) / sizeof(wide[0])) - 1 && output < max_chars) {
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
	Draw_ConsoleString(x, y, wide, NULL, 0, false, 1, false);
}

static void SCR_MyBindsDrawKey(int x, int y, const char *text)
{
	wchar wide[MYBINDS_KEY_SIZE + 3];
	int input = 0, output = 0, length = (int)strlen(text);

	wide[output++] = 0x10;
	while (input < length && output < (int)(sizeof(wide) / sizeof(wide[0])) - 2) {
		int initial = input;
		wchar decoded = TextEncodingDecodeUTF8((char *)text, &input);
		if (!decoded && text[initial]) {
			decoded = (unsigned char)text[initial];
			input = initial;
		}
		if (decoded >= '0' && decoded <= '9')
			decoded = decoded - '0' + 0x12;
		wide[output++] = decoded;
		++input;
	}
	wide[output++] = 0x11;
	wide[output] = 0;
	Draw_ConsoleString(x, y, wide, NULL, 0, false, 1, false);
}

static void SCR_ShowMyBindsDown_f(void)
{
	if (SCR_MyBindsClassScope())
		Cvar_SetValue(&showmybinds, 1);
}

static void SCR_ShowMyBindsUp_f(void)
{
	Cvar_SetValue(&showmybinds, 0);
}

void SCR_CenterPrint_Clear(void)
{
	// Make sure no centerprint messages are left from previous level.
	scr_centertime_off = 0;
	memset(scr_centerstring_lines, 0, sizeof(scr_centerstring_lines));
}

void SCR_CenterPrint_Init(void)
{
	if (!host_initialized) {
		Cvar_SetCurrentGroup(CVAR_GROUP_SCREEN);
		Cvar_Register(&scr_centertime);
		Cvar_Register(&scr_centershift);
		Cvar_Register(&scr_centerspeed);
		Cvar_Register(&showmybinds);
		Cvar_ResetCurrentGroup();

		Cmd_AddLegacyCommand("scr_printspeed", "scr_centerspeed");
		Cmd_AddCommand("+showmybinds", SCR_ShowMyBindsDown_f);
		Cmd_AddCommand("-showmybinds", SCR_ShowMyBindsUp_f);
	}
}

// Called for important messages that should stay in the center of the screen for a few moments
void SCR_CenterPrint(const char *str)
{
	scr_centertime_off = scr_centertime.value;
	scr_centertime_start = cl.time;
	memset(scr_centerstring_lines, 0, sizeof(scr_centerstring_lines));

	// count the number of lines for centering
	scr_center_lines = 0;
	while (*str) {
		const char* endl = strchr(str, '\n');
		if (!endl) {
			strlcpy(scr_centerstring_lines[scr_center_lines], str, sizeof(scr_centerstring_lines[scr_center_lines]));
			++scr_center_lines;
			break;
		}
		else {
			int len = endl - str;
			len = min(len, sizeof(scr_centerstring_lines[scr_center_lines]) - 1);
			strlcpy(scr_centerstring_lines[scr_center_lines], str, sizeof(scr_centerstring_lines[scr_center_lines]));
			scr_centerstring_lines[scr_center_lines][len] = '\0';
			++scr_center_lines;

			str = endl + 1;
		}
	}
}

static void SCR_DrawCenterString(float x, float y, float scale, qbool proportional, float speed)
{
	// the finale prints the characters one at a time
	int remaining = cl.intermission ? speed * (cl.time - scr_centertime_start) : -1;
	int l;
	float max_width = (sizeof(scr_centerstring_lines[l]) - 1) * 8 * scale;

	scale = max(scale, 0.1);
	scr_erase_center = 0;
	if (remaining == 0) {
		return;
	}

	for (l = 0; l < scr_center_lines; ++l) {
		if (remaining >= 0 && remaining < strlen(scr_centerstring_lines[l])) {
			// Can't use standard centering here... we center to the full line, we might print less than that...
			char temp[1024];
			int len;

			strlcpy(temp, scr_centerstring_lines[l], sizeof(temp));
			temp[remaining] = '\0';
			len = Draw_StringLength(scr_centerstring_lines[l], -1, scale, proportional);

			Draw_SString(x + (max_width - len) / 2, y, temp, scale, proportional);
		}
		else {
			Draw_SStringAligned(x, y, scr_centerstring_lines[l], scale, 1.0f, proportional, text_align_center, x + max_width);
		}

		if (remaining >= 0) {
			remaining -= strlen(scr_centerstring_lines[l]);
			if (remaining <= 0) {
				break;
			}
		}
		y += 8 * scale;
	}
}

static qbool SCR_CheckDrawCenterString(void)
{
	scr_copytop = 1;
	if (scr_center_lines > scr_erase_lines) {
		scr_erase_lines = scr_center_lines;
	}

	if (!scr_center_lines) {
		return false;
	}

	scr_centertime_off -= cls.frametime;
	if (scr_centertime_off <= 0 && !cl.intermission) {
		return false;
	}

	// condition says: "Draw center string only when in game or in proxy menu, otherwise leave."
	if (key_dest != key_game && ((key_dest != key_menu) || (m_state != m_proxy))) {
		return false;
	}

	return true;
}

void SCR_CenterString_Draw(void)
{
	float y;
	float max_width;
	float scale = 1.0f;
	qbool proportional = false;
	extern cvar_t scr_newHud;
	static cvar_t* hud_draw = NULL;

	if (!SCR_CheckDrawCenterString()) {
		return;
	}
	if (showmybinds.integer && SCR_MyBindsClassScope()) {
		return;
	}

	if (hud_draw == NULL) {
		hud_draw = Cvar_Find("hud_centerprint_show");
	}

	if (scr_newHud.integer > 0 && hud_draw && hud_draw->integer) {
		return;
	}

	// shift all centerprint but not proxy menu - more user-friendly way
	y = ((scr_center_lines <= 4) ? vid.height * 0.35 : 48);
	if (m_state != m_proxy) {
		y += scr_centershift.value * 8 * scale;
	}
	max_width = (sizeof(scr_centerstring_lines[0]) - 1) * 8 * scale;

	SCR_DrawCenterString((vid.width - max_width) / 2, y, scale, proportional, max(scr_centerspeed.value, 1));
}

void SCR_MyBinds_Draw(void)
{
	mybinds_row_t rows[MYBINDS_MAX_ROWS];
	const char *class_scope;
	wchar separator[] = { '-', 0 };
	int count, columns, rows_per_column, available_rows;
	int max_key_chars = 1, max_label_chars = 1;
	int column_width, available_column_width, label_chars;
	int gap = 32, total_width, start_x, start_y;
	int index;

	if (!showmybinds.integer || !(class_scope = SCR_MyBindsClassScope()) ||
		!SCR_MyBindsLoadDictionary())
		return;
	count = SCR_MyBindsCollect(rows, class_scope);
	if (!count)
		return;
	for (index = 0; index < count; ++index) {
		max_key_chars = max(max_key_chars, SCR_MyBindsUTF8Length(rows[index].keys));
		max_label_chars = max(max_label_chars, SCR_MyBindsUTF8Length(rows[index].label));
	}
	available_rows = max(1, (vid.height - 64) / MYBINDS_LINE_HEIGHT);
	columns = count > available_rows ? 2 : 1;
	rows_per_column = (count + columns - 1) / columns;
	available_column_width = max(120, (vid.width - 32 - (columns - 1) * gap) / columns);
	column_width = min((max_key_chars + 5 + max_label_chars) * MYBINDS_CHAR_WIDTH,
		available_column_width);
	label_chars = max(1, column_width / MYBINDS_CHAR_WIDTH - max_key_chars - 5);
	total_width = columns * column_width + (columns - 1) * gap;
	start_x = max(0, (vid.width - total_width) / 2);
	start_y = max(16, (vid.height - rows_per_column * MYBINDS_LINE_HEIGHT) / 2);

	for (index = 0; index < count; ++index) {
		int column = index / rows_per_column;
		int row = index % rows_per_column;
		int x = start_x + column * (column_width + gap);
		int y = start_y + row * MYBINDS_LINE_HEIGHT;
		int description_x = x + (max_key_chars + 5) * MYBINDS_CHAR_WIDTH;

		SCR_MyBindsDrawKey(x, y, rows[index].keys);
		Draw_ConsoleString(description_x - 2 * MYBINDS_CHAR_WIDTH, y,
			separator, NULL, 0, false, 1, false);
		SCR_MyBindsDrawUTF8(description_x, y, rows[index].label, label_chars);
	}
}

void SCR_EraseCenterString(void)
{
	int y;

	if (scr_erase_center++ > vid.numpages) {
		scr_erase_lines = 0;
		return;
	}

	y = (scr_center_lines <= 4) ? vid.height * 0.35 : 48;

	scr_copytop = 1;
	Draw_TileClear(0, y, vid.width, min(8 * scr_erase_lines, vid.height - y - 1));
}

static void SCR_HUD_DrawCenterPrint(hud_t* hud)
{
	int x = 0, y = 0;
	float width = 0, height = 0;

	static cvar_t
		*hud_scale = NULL,
		*hud_proportional,
		*hud_speed;

	if (!SCR_CheckDrawCenterString()) {
		return;
	}
	if (showmybinds.integer && SCR_MyBindsClassScope()) {
		return;
	}

	if (!hud_scale) {
		hud_scale = HUD_FindVar(hud, "scale");
		hud_proportional = HUD_FindVar(hud, "proportional");
		hud_speed = HUD_FindVar(hud, "speed");
	}

	width = 40 * 8 * hud_scale->value;
	height = 12 * 8 * hud_scale->value;

	if (height > 0 && width > 0 && HUD_PrepareDraw(hud, ceil(width), ceil(height), &x, &y)) {
		SCR_DrawCenterString(x, y, hud_scale->value, hud_proportional->value, max(hud_speed->integer, 1));
	}
}

void CenterPrint_HudInit(void)
{
	HUD_Register(
		"centerprint", NULL, "Shows alerts from server, countdowns etc.",
		HUD_PLUSMINUS | HUD_ON_FINALE, ca_active, 0, SCR_HUD_DrawCenterPrint,
		"0", "screen", "center", "center", "0", "0", "0", "0 0 0", NULL,
		"scale", "1",
		"proportional", "0",
		"speed", "8",
		NULL
	);
}
