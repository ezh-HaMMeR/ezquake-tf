/*
Copyright (C) 2011 azazello and ezQuake team

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "quakedef.h"
#include "common_draw.h"
#include "hud.h"
#include "hud_common.h"
#include "vx_stuff.h"

#define TFSTATE_INFECTED                1<<4
#define TFSTATE_BURNING                 1<<9
#define TFSTATE_TRANQUILISED            1<<15

#define SENTRY_HEALTH				0
#define SENTRY_LVL					1
#define SENTRY_AMMO_SHELLS  2
#define SENTRY_AMMO_ROCKETS 3

#define DISP_HEALTH				0
#define DISP_AMMO_SHELLS  1
#define DISP_AMMO_ROCKETS 2
#define DISP_AMMO_NAILS		3
#define DISP_AMMO_CELLS		4
#define DISP_AMMO_ARMOR		5

static void SCR_HUD_DrawFlaginfoData(hud_t* hud, int team, float scale, int align, qbool proportional)
{
	char t[128] = { 0 };

	if (cl.spectator != cl.autocam || cl.flaginfo[team].state == FLAGINFO_NOTINIT || cl.flaginfo[team].state == FLAG_ON_BASE)
		return;

	switch (cl.flaginfo[team].state) {
	case FLAG_ON_GROUND:
		snprintf(t, sizeof(t), "%d", (int)(cl.flaginfo[team].end - cl.time));
		SCR_HUD_MultiLineString(hud, t, 0, align, scale, proportional);
		break;
	case FLAG_CARRIED:
		snprintf(t, sizeof(t), "%s", cl.flaginfo[team].nickname);
		SCR_HUD_MultiLineString(hud, t, 0, align, scale, proportional);
		break;
	}
}

static void SCR_HUD_DrawBlueFlaginfoData(hud_t* hud) {
	static cvar_t* scale = NULL, * align, * proportional;

	if (scale == NULL) {
		scale = HUD_FindVar(hud, "scale");
		align = HUD_FindVar(hud, "align");
		proportional = HUD_FindVar(hud, "proportional");
	}

	SCR_HUD_DrawFlaginfoData(hud, 0, scale->value, align->value, proportional->value);
}

static void SCR_HUD_DrawRedFlaginfoData(hud_t* hud) {
	static cvar_t* scale = NULL, * align, * proportional;

	if (scale == NULL) {
		scale = HUD_FindVar(hud, "scale");
		align = HUD_FindVar(hud, "align");
		proportional = HUD_FindVar(hud, "proportional");
	}

	SCR_HUD_DrawFlaginfoData(hud, 1, scale->value, align->value, proportional->value);
}

static void SCR_HUD_DrawFlaginfoIcon(hud_t* hud, int team, float scale, qbool blink)
{
	extern mpic_t* sb_flags[2];
	int x, y;
	float alpha;

	if (cl.spectator != cl.autocam || cl.flaginfo[team].state == FLAGINFO_NOTINIT)
		return;

	float scaleval = max(scale, 0.01);

	if (!sb_flags[team]) return;
	if (!HUD_PrepareDraw(hud, sb_flags[0]->width * scaleval, sb_flags[team]->width * scaleval, &x, &y))
		return;

	switch (cl.flaginfo[team].state) {
	case FLAG_ON_BASE:
		Draw_SAlphaPic(x, y, sb_flags[team], 1.0f, scaleval);
		break;
	case FLAG_ON_GROUND:
		Draw_SAlphaPic(x, y, sb_flags[team], 0.75f, scaleval);
		break;
	case FLAG_CARRIED:
		alpha = blink ? sin(cl.time * 2) : 0.75f;
		Draw_SAlphaPic(x, y, sb_flags[team], alpha < 0.0f ? -alpha : alpha, scaleval);
		break;
	}
}

static void SCR_HUD_DrawBlueFlaginfoIcon(hud_t* hud) {
	static cvar_t* scale = NULL, * blink;

	if (scale == NULL) {
		scale = HUD_FindVar(hud, "scale");
		blink = HUD_FindVar(hud, "blink");
	}

	SCR_HUD_DrawFlaginfoIcon(hud, 0, scale->value, blink->value);
}

static void SCR_HUD_DrawRedFlaginfoIcon(hud_t* hud) {
	static cvar_t* scale = NULL, * blink;

	if (scale == NULL) {
		scale = HUD_FindVar(hud, "scale");
		blink = HUD_FindVar(hud, "blink");
	}

	SCR_HUD_DrawFlaginfoIcon(hud, 1, scale->value, blink->value);
}

static void SCR_HUD_DrawSentryStat(hud_t* hud, int type)
{
	static cvar_t* scale = NULL, * style, * digits, * align, * proportional;
	static int value;
	if (scale == NULL) {
		// first time called
		scale = HUD_FindVar(hud, "scale");
		style = HUD_FindVar(hud, "style");
		digits = HUD_FindVar(hud, "digits");
		align = HUD_FindVar(hud, "align");
		proportional = HUD_FindVar(hud, "proportional");
	}

	uint32_t sentry = cl.stats[STAT_SENTRY];
	if (cl.spectator != cl.autocam || !(sentry & 1))
		return;
	switch (type) {
	case SENTRY_LVL:					value = (sentry >> 1) & ((1u << 2) - 1); break;
	case SENTRY_HEALTH:				value = (sentry >> 3) & ((1u << 9) - 1); break;
	case SENTRY_AMMO_SHELLS:	value = (sentry >> 12) & ((1u << 8) - 1); break;
	case SENTRY_AMMO_ROCKETS: value = (sentry >> 20) & ((1u << 6) - 1); break;
	}
	SCR_HUD_DrawNum(hud, value, 0, scale->value, style->value, digits->value, align->string, proportional->integer);
}

static void SCR_HUD_DrawDispenserStat(hud_t* hud, int type)
{
	static cvar_t* scale = NULL, * style, * digits, * align, * proportional;
	static int value;
	if (scale == NULL) {
		// first time called
		scale = HUD_FindVar(hud, "scale");
		style = HUD_FindVar(hud, "style");
		digits = HUD_FindVar(hud, "digits");
		align = HUD_FindVar(hud, "align");
		proportional = HUD_FindVar(hud, "proportional");
	}

	uint32_t dispenser = cl.stats[STAT_DISP];
	uint32_t dispenser_add = cl.stats[STAT_DISPADD];
	if (cl.spectator != cl.autocam || !(dispenser & 1))
		return;
	switch (type) {
	case DISP_HEALTH:				value = (dispenser >> 1) & ((1u << 8) - 1); break;
	case DISP_AMMO_SHELLS:	value = (dispenser >> 9) & ((1u << 9) - 1); break;
	case DISP_AMMO_ROCKETS: value = (dispenser >> 18) & ((1u << 10) - 1); break;
	case DISP_AMMO_NAILS:		value = (dispenser_add >> 0) & ((1u << 9) - 1); break;
	case DISP_AMMO_CELLS:		value = (dispenser_add >> 9) & ((1u << 9) - 1); break;
	case DISP_AMMO_ARMOR:		value = (dispenser_add >> 18) & ((1u << 9) - 1); break;
	}
	SCR_HUD_DrawNum(hud, value, 0, scale->value, style->value, digits->value, align->string, proportional->integer);
}

static void SCR_HUD_DrawSentryHealth(hud_t* hud) { SCR_HUD_DrawSentryStat(hud, SENTRY_HEALTH); }
static void SCR_HUD_DrawSentryLvl(hud_t* hud) { SCR_HUD_DrawSentryStat(hud, SENTRY_LVL); }
static void SCR_HUD_DrawSentryAmmoShells(hud_t* hud) { SCR_HUD_DrawSentryStat(hud, SENTRY_AMMO_SHELLS); }
static void SCR_HUD_DrawSentryAmmoRockets(hud_t* hud) { SCR_HUD_DrawSentryStat(hud, SENTRY_AMMO_ROCKETS); }

static void SCR_HUD_DrawDispHealth(hud_t* hud) { SCR_HUD_DrawDispenserStat(hud, DISP_HEALTH); }
static void SCR_HUD_DrawDispAmmoShells(hud_t* hud) { SCR_HUD_DrawDispenserStat(hud, DISP_AMMO_SHELLS); }
static void SCR_HUD_DrawDispAmmoNails(hud_t* hud) { SCR_HUD_DrawDispenserStat(hud, DISP_AMMO_NAILS); }
static void SCR_HUD_DrawDispAmmoRockets(hud_t* hud) { SCR_HUD_DrawDispenserStat(hud, DISP_AMMO_ROCKETS); }
static void SCR_HUD_DrawDispAmmoCells(hud_t* hud) { SCR_HUD_DrawDispenserStat(hud, DISP_AMMO_CELLS); }
static void SCR_HUD_DrawDispAmmoArmor(hud_t* hud) { SCR_HUD_DrawDispenserStat(hud, DISP_AMMO_ARMOR); }

void SCR_HUD_DrawSentryIcon(hud_t* hud)
{
	extern mpic_t* sb_sentry[3];
	static cvar_t* scale = NULL;
	uint32_t sentry;
	uint32_t lvl;
	int   x, y;

	if (scale == NULL) {
		// first time called
		scale = HUD_FindVar(hud, "scale");
	}
	sentry = cl.stats[STAT_SENTRY];
	lvl = (sentry >> 1) & ((1u << 2) - 1);

	float scaleval = max(scale->value, 0.01);

	if (cl.spectator == cl.autocam && sentry & 1) {
		if (sb_sentry[lvl - 1] == NULL) return;
		if (!HUD_PrepareDraw(hud, sb_sentry[lvl - 1]->width * scaleval, sb_sentry[lvl - 1]->width * scaleval, &x, &y))
			return;

		Draw_SPic(x, y, sb_sentry[lvl - 1], scaleval);
	}
}

void SCR_HUD_DrawDispenserIcon(hud_t* hud)
{
	extern mpic_t* sb_disp;
	static cvar_t* scale = NULL;
	uint32_t disp;
	int   x, y;

	if (scale == NULL) {
		// first time called
		scale = HUD_FindVar(hud, "scale");
	}
	disp = cl.stats[STAT_DISP];

	float scaleval = max(scale->value, 0.01);

	if (cl.spectator == cl.autocam && disp & 1) {
		if (sb_disp == NULL) return;
		if (!HUD_PrepareDraw(hud, sb_disp->width * scaleval, sb_disp->height * scaleval, &x, &y))
			return;

		Draw_SPic(x, y, sb_disp, scaleval);
	}
}

void SCR_HUD_DrawDebuffIcons(hud_t* hud)
{
	extern mpic_t* sb_debuffs[5];
	static cvar_t* scale = NULL, *vertical, *spacing;
	int   x, y;

	if (scale == NULL) {
		// first time called
		scale = HUD_FindVar(hud, "scale");
		vertical = HUD_FindVar(hud, "vertical");
		spacing = HUD_FindVar(hud, "spacing");
	}

	float scaleval = max(scale->value, 0.01);

	if (cl.spectator == cl.autocam) {
		if (!HUD_PrepareDraw(hud, sb_debuffs[0]->width * scaleval, sb_debuffs[0]->height * scaleval, &x, &y))
			return;
		if (sb_debuffs[0] != NULL && cl.stats[STAT_TFSTATE] & TFSTATE_BURNING) {
			// Fire
			Draw_SPic(x, y, sb_debuffs[0], scaleval);
			if (vertical->value)
				y += sb_debuffs[0]->height * scaleval + spacing->value;
			else
				x += sb_debuffs[0]->width * scaleval + spacing->value;
		}
		if (sb_debuffs[1] != NULL && cl.stats[STAT_TFSTATE] & TFSTATE_INFECTED) {
			// Infected
			Draw_SPic(x, y, sb_debuffs[1], scaleval);
			if (vertical->value)
				y += sb_debuffs[1]->height * scaleval + spacing->value;
			else
				x += sb_debuffs[1]->width * scaleval + spacing->value;
		}
		if (sb_debuffs[2] != NULL && cl.stats[STAT_TFSTATE] & TFSTATE_TRANQUILISED) {
			// Tranquilized
			Draw_SPic(x, y, sb_debuffs[2], scaleval);
			if (vertical->value)
				y += sb_debuffs[1]->height * scaleval + spacing->value;
			else
				x += sb_debuffs[1]->width * scaleval + spacing->value;
		}
		if (sb_debuffs[3] != NULL && concussioned) {
			// Conced
			Draw_SPic(x, y, sb_debuffs[3], scaleval);
			if (vertical->value)
				y += sb_debuffs[1]->height * scaleval + spacing->value;
			else
				x += sb_debuffs[1]->width * scaleval + spacing->value;
		}
		if (sb_debuffs[4] != NULL && flashed) {
			// Flashed
			Draw_SPic(x, y, sb_debuffs[4], scaleval);
		}
	}
}

static void SCR_HUD_DrawClip(hud_t* hud)
{
	static cvar_t* scale = NULL, * style, * digits, * align, * proportional;
	static int value;
	if (scale == NULL) {
		// first time called
		scale = HUD_FindVar(hud, "scale");
		style = HUD_FindVar(hud, "style");
		digits = HUD_FindVar(hud, "digits");
		align = HUD_FindVar(hud, "align");
		proportional = HUD_FindVar(hud, "proportional");
	}

	value = HUD_Stats(STAT_CLIP);
	if (cl.spectator == cl.autocam && value != -1) {
		SCR_HUD_DrawNum(hud, value, 0, scale->value, style->value, digits->value, align->string, proportional->integer);
	}
}

static void SCR_HUD_DrawNumGren1(hud_t* hud)
{
	static cvar_t* scale = NULL, * style, * digits, * align, * proportional;
	static int value;
	if (scale == NULL) {
		// first time called
		scale = HUD_FindVar(hud, "scale");
		style = HUD_FindVar(hud, "style");
		digits = HUD_FindVar(hud, "digits");
		align = HUD_FindVar(hud, "align");
		proportional = HUD_FindVar(hud, "proportional");
	}

	value = HUD_Stats(STAT_NUMGREN1);
	if (cl.spectator == cl.autocam) {
		SCR_HUD_DrawNum(hud, value, 0, scale->value, style->value, digits->value, align->string, proportional->integer);
	}
}

static void SCR_HUD_DrawNumGren2(hud_t* hud)
{
	static cvar_t* scale = NULL, * style, * digits, * align, * proportional;
	static int value;
	if (scale == NULL) {
		// first time called
		scale = HUD_FindVar(hud, "scale");
		style = HUD_FindVar(hud, "style");
		digits = HUD_FindVar(hud, "digits");
		align = HUD_FindVar(hud, "align");
		proportional = HUD_FindVar(hud, "proportional");
	}

	value = HUD_Stats(STAT_NUMGREN2);
	if (cl.spectator == cl.autocam) {
		SCR_HUD_DrawNum(hud, value, 0, scale->value, style->value, digits->value, align->string, proportional->integer);
	}
}

void SCR_HUD_DrawGrenIcon(hud_t* hud, int num, float scale)
{
	extern mpic_t* sb_grens[11];
	int   x, y;

	scale = max(scale, 0.01);

	if (cl.spectator == cl.autocam && num != 0) {
		if (sb_grens[num] == NULL) return;
		if (!HUD_PrepareDraw(hud, sb_grens[num]->width * scale, sb_grens[num]->height * scale, &x, &y) || num == 0)
			return;

		Draw_SPic(x, y, sb_grens[num], scale);
	}
}

void SCR_HUD_DrawGrenIcon1(hud_t* hud)
{
	static cvar_t* scale = NULL;

	if (scale == NULL)  // first time called
	{
		scale = HUD_FindVar(hud, "scale");
	}
	
	SCR_HUD_DrawGrenIcon(hud, cl.stats[STAT_TPGREN1], scale->value);
}

void SCR_HUD_DrawGrenIcon2(hud_t* hud)
{
	static cvar_t* scale = NULL;

	if (scale == NULL)  // first time called
	{
		scale = HUD_FindVar(hud, "scale");
	}

	SCR_HUD_DrawGrenIcon(hud, cl.stats[STAT_TPGREN2], scale->value);
}

static void SCR_DrawTfBigClock(int x, int y, int style, int blink, float scale, const char* t)
{
	extern  mpic_t* sb_nums[2][11];
	extern  mpic_t* sb_colon/*, *sb_slash*/;
	qbool lblink = blink && ((int)(curtime * 10)) % 10 < 5;

	style = bound(0, style, 1);

	while (*t) {
		if (*t >= '0' && *t <= '9') {
			Draw_STransPic(x, y, sb_nums[style][*t - '0'], scale);
			x += 24 * scale;
		}
		else if (*t == ':') {
			if (lblink || !blink) {
				Draw_STransPic(x, y, sb_colon, scale);
			}

			x += 16 * scale;
		}
		else {
			Draw_SCharacter(x, y, *t + (style ? 128 : 0), 3 * scale);
			x += 24 * scale;
		}
		t++;
	}
}

static void SCR_DrawTfSmallClock(int x, int y, int style, int blink, float scale, const char* t, qbool proportional)
{
	qbool lblink = blink && ((int)(curtime * 10)) % 10 < 5;
	int c;

	style = bound(0, style, 3);

	while (*t) {
		c = (int)*t;
		if (c >= '0' && c <= '9') {
			if (style == 1) {
				c += 128;
			}
			else if (style == 2 || style == 3) {
				c -= 30;
			}
		}
		else if (c == ':') {
			if (style == 1 || style == 3) {
				c += 128;
			}
			if (lblink || !blink) {
				;
			}
			else {
				c = ' ';
			}
		}
		x += Draw_SCharacterP(x, y, c, scale, proportional);
		t++;
	}
}

static void SCR_HUD_DrawTfClock(hud_t* hud)
{
	int width, height;
	int x, y;
	int tens_minutes, minutes, tens_seconds, seconds;
	char t[80] = {0};

	static cvar_t
		* hud_tfclock_big = NULL,
		* hud_tfclock_style,
		* hud_tfclock_blink,
		* hud_tfclock_scale,
		* hud_tfclock_proportional;

	if (hud_tfclock_big == NULL)    // first time
	{
		hud_tfclock_big = HUD_FindVar(hud, "big");
		hud_tfclock_style = HUD_FindVar(hud, "style");
		hud_tfclock_scale = HUD_FindVar(hud, "scale");
		hud_tfclock_blink = HUD_FindVar(hud, "blink");
		hud_tfclock_proportional = HUD_FindVar(hud, "proportional");
	}

	tens_minutes = fmod(cl.tftime / 600, 6);
	minutes = fmod(cl.tftime / 60, 10);
	tens_seconds = fmod(cl.tftime / 10, 6);
	seconds = fmod(cl.tftime, 10);
	snprintf(t, sizeof(t), "%i%i:%i%i", tens_minutes, minutes, tens_seconds, seconds);
	width = SCR_GetClockStringWidth(t, hud_tfclock_big->integer, hud_tfclock_scale->value, hud_tfclock_proportional->integer);
	height = SCR_GetClockStringHeight(hud_tfclock_big->integer, hud_tfclock_scale->value);

	if (HUD_PrepareDraw(hud, width, height, &x, &y)) {
		if (hud_tfclock_big->value) {
			SCR_DrawTfBigClock(x, y, hud_tfclock_style->value, hud_tfclock_blink->value, hud_tfclock_scale->value, t);
		}
		else {
			SCR_DrawTfSmallClock(x, y, hud_tfclock_style->value, hud_tfclock_blink->value, hud_tfclock_scale->value, t, hud_tfclock_proportional->integer);
		}
	}
}

static void SCR_HUD_DrawScoreBlue(hud_t* hud)
{
	int blue, red;
	char* score = Info_ValueForKey(cl.serverinfo, "score");
	if (!score[0]) return;

	static cvar_t
		* hud_score_style = NULL,
		* hud_score_align,
		* hud_score_scale,
		* hud_score_flags,
		* hud_score_digits,
		* hud_score_proportional;

	if (hud_score_style == NULL)    // first time
	{
		hud_score_style = HUD_FindVar(hud, "style");
		hud_score_scale = HUD_FindVar(hud, "scale");
		hud_score_flags = HUD_FindVar(hud, "flags");
		hud_score_digits = HUD_FindVar(hud, "digits");
		hud_score_align = HUD_FindVar(hud, "align");
		hud_score_proportional = HUD_FindVar(hud, "proportional");
	}

	blue = red = 0;
	while (*score != ':') {
		blue *= 10;
		blue += (*score) - '0';
		score++;
		if (!(*score)) return;
	}
	score++;
	while (*score) {
		red *= 10;
		red += (*score) - '0';
		score++;
	}
	
	if (hud_score_flags->value) {
		blue /= 10;
		red /= 10;
	}
	
	if (cl.spectator == cl.autocam) {
		SCR_HUD_DrawNum(hud, blue, 0, hud_score_scale->value, hud_score_style->value, hud_score_digits->value, hud_score_align->string, hud_score_proportional->integer);
	}
}

static void SCR_HUD_DrawScoreRed(hud_t* hud)
{
	int blue, red;
	char* score = Info_ValueForKey(cl.serverinfo, "score");
	if (!score[0]) return;

	static cvar_t
		* hud_score_style = NULL,
		* hud_score_align,
		* hud_score_scale,
		* hud_score_flags,
		* hud_score_digits,
		* hud_score_proportional;

	if (hud_score_style == NULL)    // first time
	{
		hud_score_style = HUD_FindVar(hud, "style");
		hud_score_scale = HUD_FindVar(hud, "scale");
		hud_score_flags = HUD_FindVar(hud, "flags");
		hud_score_digits = HUD_FindVar(hud, "digits");
		hud_score_align = HUD_FindVar(hud, "align");
		hud_score_proportional = HUD_FindVar(hud, "proportional");
	}

	blue = red = 0;
	while (*score != ':') {
		blue *= 10;
		blue += (*score) - '0';
		score++;
		if (!(*score)) return;
	}
	score++;
	while (*score) {
		red *= 10;
		red += (*score) - '0';
		score++;
	}

	if (hud_score_flags->value) {
		blue /= 10;
		red /= 10;
	}

	if (cl.spectator == cl.autocam) {
		SCR_HUD_DrawNum(hud, red, 0, hud_score_scale->value, hud_score_style->value, hud_score_digits->value, hud_score_align->string, hud_score_proportional->integer);
	}
}

void SCR_HUD_DrawDisguiseIcon(hud_t* hud)
{
	extern mpic_t* sb_tfclass[2][9];
	static cvar_t* scale = NULL;
	uint32_t disguise;
	int team, skin;
	int   x, y;

	if (scale == NULL) {
		// first time called
		scale = HUD_FindVar(hud, "scale");
	}
	disguise = cl.stats[STAT_SPYINFO];
	team = disguise & 3;
	skin = (disguise >> 2) & 15;
	team = (team > 2 ? 0 : team);
	skin = (skin > 9 ? 0 : skin);

	float scaleval = max(scale->value, 0.01);

	if (cl.spectator == cl.autocam && team != 0 && skin != 0) {
		if (sb_tfclass[team - 1][skin - 1] == NULL) return;
		if (!HUD_PrepareDraw(hud, sb_tfclass[team - 1][skin - 1]->width * scaleval, sb_tfclass[team - 1][skin - 1]->height * scaleval, &x, &y))
			return;

		Draw_SPic(x, y, sb_tfclass[team - 1][skin - 1], scaleval);
	}
}

const double class_armor_lookup[11] = {
	0.0f, // NONE
	0.3f, // PC_SCOUT
	0.3f, // PC_SNIPER
	0.8f, // PC_SOLDIER
	0.6f, // PC_DEMOMAN
	0.6f, // PC_MEDIC
	0.8f, // PC_HVYWEAP
	0.6f, // PC_PYRO
	0.3f, // PC_SPY
	0.6f, // PC_ENGINEER
	0.0f  // PC_CIVILIAN
};
static void SCR_HUD_DrawTFEffHealth(hud_t* hud)
{
	static cvar_t* scale = NULL, * style, * digits, * align, * proportional;
	int health, armor;
	int eff_health;
	double ar_ab = 1.0f, hl_ab = 0.0f, ar_dmg, hl_dmg, dmg;
	int idx = 0;
	if (scale == NULL) {
		scale = HUD_FindVar(hud, "scale");
		style = HUD_FindVar(hud, "style");
		digits = HUD_FindVar(hud, "digits");
		align = HUD_FindVar(hud, "align");
		proportional = HUD_FindVar(hud, "proportional");
	}

	health = cl.stats[STAT_HEALTH];
	armor = cl.stats[STAT_ARMOR];
	if (cl.spectator == cl.autocam) {
		idx = cl.ideal_track;
	} else {
		idx = cl.playernum;
	}

	assert(cl.players[idx].playerclass >= 0 && cl.players[idx].playerclass <= 10);
	ar_ab = class_armor_lookup[cl.players[idx].playerclass];
	hl_ab = 1.0f - ar_ab;
	dmg = (double)armor / ar_ab;
	ar_dmg = dmg * ar_ab;
	hl_dmg = dmg * hl_ab;
	if (hl_dmg > health) {
		eff_health = (double)health / hl_ab;
	} else {
		eff_health = ar_dmg + health;
	}

	if (eff_health < 0) {
		eff_health = 0;
	}

	SCR_HUD_DrawNum(hud, eff_health, 0, scale->value, style->value, digits->value, align->string, proportional->integer);
}

void TF_HudInit(void)
{
	HUD_Register(
		"blueflaginfodata", NULL, "Blue's flag's info data",
		HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawBlueFlaginfoData,
		"1", "screen", "center", "bottom", "0", "0", "0", "0 0 0", NULL,
		"scale", "1",
		"align", "right",
		"proportional", "0",
		NULL
	);

	HUD_Register(
		"redflaginfodata", NULL, "Red's flag's info data",
		HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawRedFlaginfoData,
		"1", "screen", "center", "bottom", "0", "0", "0", "0 0 0", NULL,
		"scale", "1",
		"align", "right",
		"proportional", "0",
		NULL
	);

	HUD_Register(
		"blueflaginfoicon", NULL, "Blue flag's info icon",
		0, ca_active, 0, SCR_HUD_DrawBlueFlaginfoIcon,
		"0", "screen", "center", "bottom", "0", "0", "0", "0 0 0", NULL,
		"scale", "0.25",
		"blink", "1",
		NULL
	);

	HUD_Register(
		"redflaginfoicon", NULL, "Red flag's info icon",
		0, ca_active, 0, SCR_HUD_DrawRedFlaginfoIcon,
		"0", "screen", "center", "bottom", "0", "0", "0", "0 0 0", NULL,
		"scale", "0.25",
		"blink", "1",
		NULL
	);

	HUD_Register("disguiseicon", NULL, "TF disguise icon",
		HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawDisguiseIcon,
		"0", "ibar", "left", "top", "0", "0", "0", "0 0 0", NULL,
		"scale", "0.25",
		"proportional", "0",
		NULL);

	HUD_Register("debufficons", NULL, "TF debuff icons",
		HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawDebuffIcons,
		"0", "ibar", "left", "top", "0", "0", "0", "0 0 0", NULL,
		"scale", "0.25",
		"proportional", "0",
		"vertical", "0",
		"spacing", "8",
		NULL);

	HUD_Register(
		"sentryhealth", NULL, "",
		HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawSentryHealth,
		"1", "face", "after", "center", "0", "0", "0", "0 0 0", NULL,
		"style", "0",
		"scale", "1",
		"align", "right",
		"digits", "3",
		"proportional", "0",
		NULL
	);

	HUD_Register(
		"sentrylvl", NULL, "",
		HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawSentryLvl,
		"1", "face", "after", "center", "0", "0", "0", "0 0 0", NULL,
		"style", "0",
		"scale", "1",
		"align", "right",
		"digits", "3",
		"proportional", "0",
		NULL
	);

	HUD_Register(
		"sentryshells", NULL, "",
		HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawSentryAmmoShells,
		"1", "face", "after", "center", "0", "0", "0", "0 0 0", NULL,
		"style", "0",
		"scale", "1",
		"align", "right",
		"digits", "3",
		"proportional", "0",
		NULL
	);

	HUD_Register(
		"sentryrockets", NULL, "",
		HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawSentryAmmoRockets,
		"1", "face", "after", "center", "0", "0", "0", "0 0 0", NULL,
		"style", "0",
		"scale", "1",
		"align", "right",
		"digits", "3",
		"proportional", "0",
		NULL
	);

	HUD_Register(
		"disphealth", NULL, "",
		HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawDispHealth,
		"1", "face", "after", "center", "0", "0", "0", "0 0 0", NULL,
		"style", "0",
		"scale", "1",
		"align", "right",
		"digits", "3",
		"proportional", "0",
		NULL
	);

	HUD_Register(
		"dispshells", NULL, "",
		HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawDispAmmoShells,
		"1", "face", "after", "center", "0", "0", "0", "0 0 0", NULL,
		"style", "0",
		"scale", "1",
		"align", "right",
		"digits", "3",
		"proportional", "0",
		NULL
	);

	HUD_Register(
		"dispnails", NULL, "",
		HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawDispAmmoNails,
		"1", "face", "after", "center", "0", "0", "0", "0 0 0", NULL,
		"style", "0",
		"scale", "1",
		"align", "right",
		"digits", "3",
		"proportional", "0",
		NULL
	);

	HUD_Register(
		"disprockets", NULL, "",
		HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawDispAmmoRockets,
		"1", "face", "after", "center", "0", "0", "0", "0 0 0", NULL,
		"style", "0",
		"scale", "1",
		"align", "right",
		"digits", "3",
		"proportional", "0",
		NULL
	);

	HUD_Register(
		"dispcells", NULL, "",
		HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawDispAmmoCells,
		"1", "face", "after", "center", "0", "0", "0", "0 0 0", NULL,
		"style", "0",
		"scale", "1",
		"align", "right",
		"digits", "3",
		"proportional", "0",
		NULL
	);

	HUD_Register(
		"disparmor", NULL, "",
		HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawDispAmmoArmor,
		"1", "face", "after", "center", "0", "0", "0", "0 0 0", NULL,
		"style", "0",
		"scale", "1",
		"align", "right",
		"digits", "3",
		"proportional", "0",
		NULL
	);

	HUD_Register("sentryicon", NULL, "TF sentry icon",
		HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawSentryIcon,
		"0", "ibar", "left", "top", "0", "0", "0", "0 0 0", NULL,
		"scale", "0.25",
		"proportional", "0",
		NULL);

	HUD_Register("dispicon", NULL, "TF dispenser icon",
		HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawDispenserIcon,
		"0", "ibar", "left", "top", "0", "0", "0", "0 0 0", NULL,
		"scale", "0.25",
		"proportional", "0",
		NULL);

	HUD_Register(
		"scoreblue", NULL, "Shows current game time (mm:ss).",
		HUD_PLUSMINUS, ca_active, 8, SCR_HUD_DrawScoreBlue,
		"1", "top", "right", "console", "0", "0", "0", "0 0 0", NULL,
		"style", "0",
		"scale", "1",
		"align", "right",
		"digits", "3",
		"proportional", "0",
		"flags", "1",
		NULL
	);

	HUD_Register(
		"scorered", NULL, "Shows current game time (mm:ss).",
		HUD_PLUSMINUS, ca_active, 8, SCR_HUD_DrawScoreRed,
		"1", "top", "right", "console", "0", "0", "0", "0 0 0", NULL,
		"style", "0",
		"scale", "1",
		"align", "right",
		"digits", "3",
		"proportional", "0",
		"flags", "1",
		NULL
	);

	HUD_Register(
		"tfclock", NULL, "Shows current game time (mm:ss).",
		HUD_PLUSMINUS, ca_disconnected, 8, SCR_HUD_DrawTfClock,
		"1", "top", "right", "console", "0", "0", "0", "0 0 0", NULL,
		"big", "1",
		"style", "0",
		"scale", "1",
		"blink", "1",
		"proportional", "0",
		NULL
	);

	// clip
	HUD_Register(
		"clip", NULL, "TF current clip",
		HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawClip,
		"1", "face", "after", "center", "0", "0", "0", "0 0 0", NULL,
		"style", "0",
		"scale", "1",
		"align", "right",
		"digits", "3",
		"proportional", "0",
		NULL
	);

	HUD_Register(
		"effhealth", NULL, "Effective health",
		HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawTFEffHealth,
		"1", "face", "after", "center", "0", "0", "0", "0 0 0", NULL,
		"style", "0",
		"scale", "1",
		"align", "right",
		"digits", "3",
		"proportional", "0",
		NULL
	);

	// numgren1
	HUD_Register(
		"numgren1", NULL, "Part of your status - number of grenades of first type.",
		HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawNumGren1,
		"1", "face", "after", "center", "0", "0", "0", "0 0 0", NULL,
		"style", "0",
		"scale", "1",
		"align", "right",
		"digits", "3",
		"proportional", "0",
		NULL
	);

	// numgren2
	HUD_Register(
		"numgren2", NULL, "Part of your status - number of grenades of second type.",
		HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawNumGren2,
		"1", "face", "after", "center", "0", "0", "0", "0 0 0", NULL,
		"style", "0",
		"scale", "1",
		"align", "right",
		"digits", "3",
		"proportional", "0",
		NULL
	);

	HUD_Register("tpgren1", NULL, "Grenade type 1 icon",
		HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawGrenIcon1,
		"0", "ibar", "left", "top", "0", "0", "0", "0 0 0", NULL,
		"scale", "0.25",
		"proportional", "0",
		NULL);

	HUD_Register("tpgren2", NULL, "Grenade type 2 icon",
		HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawGrenIcon2,
		"0", "ibar", "left", "top", "0", "0", "0", "0 0 0", NULL,
		"scale", "0.25",
		"proportional", "0",
		NULL);
}
