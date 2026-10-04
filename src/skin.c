/*
Copyright (C) 1996-1997 Id Software, Inc.

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

	$Id: skin.c,v 1.24 2007-10-04 15:52:04 dkure Exp $
*/

#include "quakedef.h"
#include "gl_model.h"
#include "teamplay.h"
#include "image.h"
#include "qtv.h"
#include "utils.h"
#include "tr_types.h"
#include "r_texture.h"
#include "r_local.h"
#include "rulesets.h"

extern cvar_t gl_playermip;
extern cvar_t gl_nocolors;
extern cvar_t gl_no24bit;

typedef struct player_skin_s {
	texture_ref base;       // The standard skin.
	texture_ref fb;         // Fullbright - for PCX textures only?
	texture_ref dead;       // Skin to use for corpses (if null, use base instead)

	qbool owned[3];
} player_skin_t;

static player_skin_t playerskins[MAX_CLIENTS];
static qbool skins_need_preache = true;

static byte* Skin_Cache(skin_t *skin, qbool no_baseskin);
static void Skin_Blend(byte* original, skin_t* skin, int skin_number);

void OnChangeSkinForcing(cvar_t *var, char *string, qbool *cancel);

cvar_t	noskins = { "noskins", "0", CVAR_RELOAD_GFX };

cvar_t  enemyforceskins     = {"enemyforceskins", "0", 0, OnChangeSkinForcing};
cvar_t  teamforceskins      = {"teamforceskins", "0", 0, OnChangeSkinForcing};

static cvar_t  baseskin = { "baseskin", "base", CVAR_RELOAD_GFX };
static cvar_t  cl_name_as_skin     = {"cl_name_as_skin", "0", 0, OnChangeSkinForcing};
cvar_t  r_enemyskincolor    = {"r_enemyskincolor", "", CVAR_COLOR, OnChangeSkinForcing};
cvar_t  r_teamskincolor     = {"r_teamskincolor",  "", CVAR_COLOR, OnChangeSkinForcing};
static cvar_t  r_skincolormode     = {"r_skincolormode",  "0", 0, OnChangeSkinForcing};
static cvar_t  r_skincolormodedead = {"r_skincolormodedead", "-1", 0, OnChangeSkinForcing};
cvar_t  r_fullbrightSkins = { "r_fullbrightSkins", "1", 0, Rulesets_OnChange_r_fullbrightSkins };

char	allskins[MAX_OSPATH];

#define	MAX_CACHED_SKINS	128
static skin_t	skins[MAX_CACHED_SKINS];

static int numskins;

// return type of skin forcing if it's allowed for the player in the POV
static int Skin_ForcingType(const char *team)
{
	if (teamforceskins.integer && TP_ThisPOV_IsHisTeam(team)) {
		return teamforceskins.integer;	// allow for teammates
	}

	if (enemyforceskins.integer && !TP_ThisPOV_IsHisTeam(team)) {
		if (cls.demoplayback || cl.spectator) { // allow for demos
			return enemyforceskins.integer;
		}
		else {  // for gameplay respect the FPD
			if (!(cl.fpd & FPD_NO_FORCE_SKIN) && !(cl.fpd & FPD_NO_FORCE_COLOR)) {
				return enemyforceskins.integer;
			}
			else {
				return 0;
			}
		}
	}

	// this was always only for demos
	if (cl_name_as_skin.integer && (cls.demoplayback || cl.spectator)) {
		return cl_name_as_skin.integer;
	}

	return 0;
}

// get player skin as player name or player id
static char* Skin_AsNameOrId(player_info_t *sc)
{
	static char name[MAX_OSPATH];
	int pn;
	char* mask;

	switch (Skin_ForcingType(sc->team)) {
	case 1: // get skin as player name
		Util_ToValidFileName(sc->name, name, sizeof(name));
		Q_strlwr(name);
		return name;
		break;

	case 2: // get skin as id
		snprintf(name, sizeof(name), "%d", sc->userid);
		return name;
		break;

	case 3: // get player's number (first teammate gets 1, second 2, ...)
		pn = TP_PlayersNumber(sc->userid, sc->team);
		if (pn) {
			mask = TP_ThisPOV_IsHisTeam(sc->team) ? "t%d" : "e%d";
			snprintf(name, sizeof(name), mask, pn);
			return name;
		}
		break;
	}

	return NULL;
}

static char* Skin_FindName(player_info_t* sc, qbool* is_teammate)
{
	static char name[MAX_OSPATH];

	if (is_teammate) {
		*is_teammate = false;
	}
	if (allskins[0]) {
		strlcpy(name, allskins, sizeof(name));
	}
	else {
		char *s = Skin_AsNameOrId(sc);

		if (!s || !s[0]) {
			s = Info_ValueForKey(sc->userinfo, "skin");
		}

		if (s && s[0]) {
			strlcpy(name, s, sizeof(name));
		}
		else {
			strlcpy(name, baseskin.string, sizeof(name));
		}
	}

	skinforcing_team = TP_SkinForcingTeam();

	if (!cl.teamfortress && !(cl.fpd & FPD_NO_FORCE_SKIN)) {
		char *skinname = NULL;
		player_state_t *state;
		qbool teammate = (cl.teamplay && !strcmp(sc->team, skinforcing_team));
		if (is_teammate) {
			*is_teammate = teammate;
		}

		if (!cl.validsequence) {
			goto nopowerups;
		}

		state = cl.frames[cl.parsecount & UPDATE_MASK].playerstate + (sc - cl.players);

		if (state->messagenum != cl.parsecount) {
			goto nopowerups;
		}

		if ((state->effects & (EF_BLUE | EF_RED)) == (EF_BLUE | EF_RED)) {
			skinname = teammate ? cl_teambothskin.string : cl_enemybothskin.string;
		}
		else if (state->effects & EF_BLUE) {
			skinname = teammate ? cl_teamquadskin.string : cl_enemyquadskin.string;
		}
		else if (state->effects & EF_RED) {
			skinname = teammate ? cl_teampentskin.string : cl_enemypentskin.string;
		}

	nopowerups:
		if (!skinname || !skinname[0]) {
			skinname = teammate ? cl_teamskin.string : cl_enemyskin.string;
		}
		if (skinname[0]) {
			strlcpy(name, skinname, sizeof(name));
		}
	}

	if (strstr(name, "..") || *name == '.') {
		strlcpy(name, baseskin.string, sizeof(name));
	}

	// EDIT TO FIT NEW TFINFO
	if (sc->playerclass == PC_SPY && cl.players[cl.playernum].team_no == sc->team_no) {
		return "tf_spy";
	}

	return name;
}

//Determines the best skin for the given scoreboard slot, and sets scoreboard->skin
static void Skin_Find_Ex(player_info_t *sc, char *skin_name)
{
	skin_t *skin;
	int i;
	char name[MAX_OSPATH];

	if (!skin_name || !skin_name[0]) {
		skin_name = baseskin.string;

		if (!skin_name[0]) {
			skin_name = "base";
		}
	}

	strlcpy(name, skin_name, sizeof(name));
	COM_StripExtension(name, name, sizeof(name));

	for (i = 0; i < numskins; i++) {
		if (!strcmp(name, skins[i].name)) {
			if (sc) {
				sc->skin = &skins[i];
			}

			return;
		}
	}

	if (numskins == MAX_CACHED_SKINS) {
		// ran out of spots, so flush everything
		Com_Printf("MAX_CACHED_SKINS reached, flushing skins\n");
		Skin_Skins_f(); // this must set numskins to 0
	}

	skin = &skins[numskins];
	if (sc) {
		sc->skin = skin;
	}
	numskins++;

	memset(skin, 0, sizeof(*skin));
	strlcpy(skin->name, name, sizeof(skin->name));
}

static void Skin_Find(player_info_t *sc)
{
	Skin_Find_Ex(sc, Skin_FindName(sc, NULL));
}

static byte* Skin_PixelsLoad(char *name, int *max_w, int *max_h, int *bpp, int *real_width, int *real_height)
{
	byte *pic;

	*max_w = *max_h = *bpp = 0;

	// PCX skins loads different, so using TEX_NO_PCX
	if (gl_no24bit.integer == 0 && (pic = R_LoadImagePixels(name, 0, 0, TEX_NO_PCX, real_width, real_height))) {
		// No limit in gl.
		*max_w = *real_width;
		*max_h = *real_height;
		*bpp = 4; // 32 bit.

		return pic;
	}

	if ((pic = Image_LoadPCX(NULL, name, 0, 0, real_width, real_height))) {
		// PCX is limited.
		*max_w = 320;
		*max_h = 200;
		*bpp = 1; // 8 bit

		return pic;
	}

	return NULL;
}

// "HACK"
// Called before rendering scene, chance to load textures again if skin
//   rules have changed
void Skins_PreCache(void)
{
	int i;
	byte *tex;

	if (!skins_need_preache) {
		// no need, we have all skins we need
		return;
	}

	skins_need_preache = false;

	// this must register skins with such names in skins[] array
	Skin_Find_Ex(NULL, cl_teamskin.string);
	Skin_Find_Ex(NULL, cl_enemyskin.string);
	Skin_Find_Ex(NULL, cl_teamquadskin.string);
	Skin_Find_Ex(NULL, cl_enemyquadskin.string);
	Skin_Find_Ex(NULL, cl_teampentskin.string);
	Skin_Find_Ex(NULL, cl_enemypentskin.string);
	Skin_Find_Ex(NULL, cl_teambothskin.string);
	Skin_Find_Ex(NULL, cl_enemybothskin.string);
	Skin_Find_Ex(NULL, baseskin.string);
	Skin_Find_Ex(NULL, "base");

	// now load all 24 bit skins in skins[] array
	for (i = 0; i < numskins; i++) {
		tex = Skin_Cache(&skins[i], false); // this precache skin file in mem

		if (!tex) {
			continue; // nothing more we can do
		}

		if (skins[i].bpp != 4) {
			// we interested in 24 bit skins only
			continue;
		}

		if (R_TextureReferenceIsValid(skins[i].texnum[skin_base])) {
			// seems skin alredy loaded, at least we have some texture
			continue;
		}

		Skin_Blend(tex, &skins[i], i);

		Com_DPrintf("skin precache: %s, texnum %d\n", skins[i].name, skins[i].texnum[skin_base]);
	}
}

// Returns a pointer to the skin bitmap, or NULL to use the default
static byte* Skin_Cache(skin_t *skin, qbool no_baseskin)
{
	int y, max_w, max_h, bpp, real_width = -1, real_height = -1;
	byte *pic = NULL, *out, *pix;
	char name[MAX_OSPATH];

	// JACK: So NOSKINS > 1 will show skins, but not download new ones.
	if (noskins.integer == 1) {
		return NULL;
	}

	if (skin->failedload) {
		return NULL;
	}

	if (skin->cached_data) {
		return skin->cached_data;
	}

	// not cached, load from HDD
	snprintf(name, sizeof(name), "skins/%s.pcx", skin->name);
	if (!(pic = Skin_PixelsLoad(name, &max_w, &max_h, &bpp, &real_width, &real_height)) || real_width > max_w || real_height > max_h) {
		Q_free(pic);

		if (no_baseskin) {
			skin->warned = true;
			return NULL; // well, we not set skin->failedload = true, that how I need it here
		}
		else if (!skin->warned && strcmp(skin->name, "base")) {
			Com_Printf("&cf22Couldn't load skin:&r %s\n", name);
		}

		skin->warned = true;
	}

	if (!pic) {
		// Attempt load at least default/base.
		snprintf(name, sizeof(name), "skins/%s.pcx", baseskin.string);

		if (!(pic = Skin_PixelsLoad(name, &max_w, &max_h, &bpp, &real_width, &real_height)) || real_width > max_w || real_height > max_h) {
			Q_free(pic);
			skin->failedload = true;
			return NULL;
		}
	}

	if (!(out = pix = skin->cached_data = (byte *)Q_malloc_named(max_w * max_h * bpp, skin->name))) {
		Sys_Error("Skin_Cache: couldn't allocate");
	}

	memset(out, 0, max_w * max_h * bpp);
	for (y = 0; y < real_height; y++, pix += (max_w * bpp)) {
		memcpy(pix, pic + y * real_width * bpp, real_width * bpp);
	}

	Q_free(pic);
	skin->bpp = bpp;
	skin->width = real_width;
	skin->height = real_height;

	// Comments about not requiring cache for 24-bit skins removed...
	//   now each player gets their own copy
	skin->failedload = false;

	return out;
}

void Skin_NextDownload(void)
{
	player_info_t *sc;
	int i;

	if (cls.downloadnumber == 0)
		if (!com_serveractive || developer.value)
			Com_DPrintf("Checking skins...\n");

	cls.downloadtype = dl_skin;

	for (; cls.downloadnumber >= 0 && cls.downloadnumber < MAX_CLIENTS; cls.downloadnumber++) {
		sc = &cl.players[cls.downloadnumber];
		if (!sc->name[0]) {
			continue;
		}

		Skin_Find(sc);

		if (noskins.integer) {
			continue;
		}

		if (Skin_Cache(sc->skin, true)) {
			continue; // we have it in cache, that mean we somehow able load this skin
		}

		if (!CL_CheckOrDownloadFile(va("skins/%s.pcx", sc->skin->name))) {
			return;   // started a download
		}
	}

	cls.downloadtype = dl_none;

	// now load them in for real
	for (i = 0; i < MAX_CLIENTS; i++) {
		sc = &cl.players[i];
		if (!sc->name[0]) {
			continue;
		}

		if (!sc->skin) {
			Skin_Find(sc);
		}

		Skin_Cache(sc->skin, false);
		sc->skin = NULL; // this way triggered skin loading, as i understand in R_TranslatePlayerSkin()
	}

	if (cls.state == ca_onserver /* && cbuf_current != &cbuf_main */) {	//only download when connecting
		MSG_WriteByte(&cls.netchan.message, clc_stringcmd);
		MSG_WriteString(&cls.netchan.message, va("begin %i", cl.servercount));
	}
}

void Skin_Clear(qbool download)
{
	int i;

	for (i = 0; i < numskins; i++) {
		Q_free(skins[i].cached_data);
	}
	numskins = 0;

	skins_need_preache = true; // we need precache it ASAP

	if (download) {
		cls.downloadnumber = 0;
		cls.downloadtype = dl_skin;
		Skin_NextDownload();

		if (cls.mvdplayback == QTV_PLAYBACK && cbuf_current != &cbuf_main) {
			cls.qtv_donotbuffer = false;
		}
	}
}

//Refind all skins, downloading if needed.
void Skin_Skins_f(void)
{
	Skin_Clear(true);
}

//Sets all skins to one specific one
void Skin_AllSkins_f(void)
{
	if (Cmd_Argc() == 1) {
		Com_Printf("allskins set to \"%s\"\n", allskins);
		return;
	}
	if (Cmd_Argc() != 2) {
		Com_Printf("Usage: %s [skin]\n", Cmd_Argv(0));
		return;
	}
	strlcpy(allskins, Cmd_Argv(1), sizeof(allskins));
	Skin_Skins_f();
}

//Just show skins which ezquake assign to each player, that depends on alot of variables and different conditions,
//so may be useful for checking settings
void Skin_ShowSkins_f(void)
{
	int i, count, maxlen;

	maxlen = sizeof("name") - 1;

	for (i = 0; i < MAX_CLIENTS; i++) {
		if (cl.players[i].name[0] && !cl.players[i].spectator) {
			// get len of longest name, but no more than 17
			maxlen = bound(maxlen, strlen(cl.players[i].name), 17);
		}
	}

	for (i = count = 0; i < MAX_CLIENTS; i++) {
		if (cl.players[i].name[0] && !cl.players[i].spectator) {
			if (!count) {
				Com_Printf("\x02%-*.*s %s\n", maxlen, maxlen, "name", "skin");
			}

			Com_Printf("%-*.*s %s\n", maxlen, maxlen, cl.players[i].name, Skin_FindName(&cl.players[i], NULL));

			count++;
		}
	}
}

static texture_ref Skin_ApplyRGBColor(byte* original, int width, int height, byte* specific, byte* color, int mode, const char* texture_name)
{
	int x, y;
	float fColor[3];

	if (color) {
		VectorScale(color, 1 / 255.0f, fColor);
	}
	else {
		VectorSet(fColor, 1, 1, 1);
	}
	memcpy(specific, original, width * height * 4);
	for (x = 0; x < width; ++x) {
		for (y = 0; y < height; ++y) {
			byte* src = &original[(x + y * width) * 4];
			byte* dst = &specific[(x + y * width) * 4];

			switch (mode) {
				case 0:
					// Solid colour
					if (color) {
						VectorCopy(color, dst);
					}
					break;
				case 1: // GL_REPLACE, no action
				case 3: // GL_DECAL... should be affected by alpha but it's RGB, so it's the same (?)
					break;
				case 2: // GL_BLEND
					dst[0] = fColor[0] * (255 - src[0]);
					dst[1] = fColor[1] * (255 - src[1]);
					dst[2] = fColor[2] * (255 - src[2]);
					break;
				case 4: // GL_ADD
					if (color) {
						dst[0] = min(255, src[0] + color[0]);
						dst[1] = min(255, src[1] + color[1]);
						dst[2] = min(255, src[2] + color[2]);
					}
					break;
				default: // GL_MODULATE
					dst[0] = fColor[0] * src[0];
					dst[1] = fColor[1] * src[1];
					dst[2] = fColor[2] * src[2];
					break;
			}
		}
	}

	return R_LoadTexture(texture_name, width, height, specific, TEX_MIPMAP | TEX_NOSCALE, 4);
}

static void Skin_Blend(byte* original, skin_t* skin, int skin_number)
{
	const char* types[skin_textures] = { "$skin-base", "$skin-base-tm", "$skin-fb", "$skin-dead", "$skin-dead-tm" };
	qbool teammates[skin_textures] = { false, true, false, false, true };
	int i;
	byte* specific;
	char texture_name[128];
	qbool block_adjustments = (cl.teamfortress || (cl.fpd & (FPD_NO_FORCE_SKIN | FPD_NO_FORCE_COLOR)));

	// FIXME: Delete old, don't leave lying around.  Watch out for solidtexture references
	memset(skin->texnum, 0, sizeof(skin->texnum));

	// If config says no color adjustments, load as normal skin
	block_adjustments |= (!r_teamskincolor.string[0] && !r_enemyskincolor.string[0]) || (r_skincolormodedead.integer <= 0 && r_skincolormode.integer == 0);
	if (block_adjustments) {
		snprintf(texture_name, sizeof(texture_name), "%s-%02d", types[skin_base], skin_number);
		skin->texnum[skin_base] = R_LoadTexture(texture_name, skin->width, skin->height, original, TEX_MIPMAP | TEX_NOSCALE, 4);
		return;
	}

	specific = Q_malloc(skin->width * skin->height * 4);
	for (i = 0; i < skin_textures; ++i) {
		cvar_t* color = teammates[i] ? &r_teamskincolor : &r_enemyskincolor;
		int mode = (i == skin_dead || i == skin_dead_teammate) && r_skincolormodedead.integer >= 0 ? r_skincolormodedead.integer : r_skincolormode.integer;

		snprintf(texture_name, sizeof(texture_name), "%s-%02d", types[i], skin_number);
		if (!color->string[0]) {
			// If no color adjustment required then we only need one version
			if (i == skin_base) {
				// Load normal texture
				skin->texnum[skin_base] = R_LoadTexture(texture_name, skin->width, skin->height, original, TEX_MIPMAP | TEX_NOSCALE, 4);
			}
			continue;
		}

		// Modify the original as per mapping to OpenGL functions
		skin->texnum[i] = Skin_ApplyRGBColor(original, skin->width, skin->height, specific, color->color, mode, texture_name);
	}
	Q_free(specific);
}

static void Skin_RemoveSkinsForPlayer(int playernum)
{
	if (playerskins[playernum].owned[0]) {
		R_DeleteTexture(&playerskins[playernum].base);
	}
	if (playerskins[playernum].owned[1]) {
		R_DeleteTexture(&playerskins[playernum].fb);
	}
	if (playerskins[playernum].owned[2]) {
		R_DeleteTexture(&playerskins[playernum].dead);
	}
	R_TextureReferenceInvalidate(playerskins[playernum].base);
	R_TextureReferenceInvalidate(playerskins[playernum].fb);
	R_TextureReferenceInvalidate(playerskins[playernum].dead);
	memset(playerskins[playernum].owned, 0, sizeof(playerskins[playernum].owned));
}

static void R_BlendPlayerSkin(skin_t* skin, qbool teammate, int playernum, byte* original, int width, int height, qbool fullbright)
{
	cvar_t* color = teammate ? &r_teamskincolor : &r_enemyskincolor;
	byte* specific = Q_malloc(width * height * 4);
	texture_ref blended;
	char texture_name[128];

	snprintf(texture_name, sizeof(texture_name), "$player-skin-%d", playernum);
	blended = Skin_ApplyRGBColor(original, width, height, specific, color->string[0] ? color->color : NULL, r_skincolormode.integer, texture_name);
	if (fullbright) {
		playerskins[playernum].fb = blended;
		playerskins[playernum].owned[1] = true;
	}
	else {
		playerskins[playernum].base = blended;
		playerskins[playernum].owned[0] = true;

		if (r_skincolormodedead.integer >= 0 && r_skincolormodedead.integer != r_skincolormode.integer) {
			strlcat(texture_name, "-dead", sizeof(texture_name));
			playerskins[playernum].dead = Skin_ApplyRGBColor(original, width, height, specific, color->color, r_skincolormodedead.integer, texture_name);
			playerskins[playernum].owned[2] = true;
		}
	}
	Q_free(specific);
}

static void Skin_SetRGBTranslationRamp(unsigned translate32[256], int range, const byte rgb[3])
{
	int i, peak = 1;

	for (i = 0; i < 16; i++) {
		byte *shade = (byte *)&d_8to24table[range + i];
		peak = max(peak, max(shade[0], max(shade[1], shade[2])));
	}
	for (i = 0; i < 16; i++) {
		unsigned color = 0;
		byte *shade = (byte *)&d_8to24table[range + i];
		byte *out = (byte *)&color;
		int brightness = max(shade[0], max(shade[1], shade[2]));

		out[0] = rgb[0] * brightness / peak;
		out[1] = rgb[1] * brightness / peak;
		out[2] = rgb[2] * brightness / peak;
		out[3] = 255;
		translate32[range + i] = color;
	}
}

/* Opt-in HD TF skins use a companion mask. Unrelated 32-bit skins retain
 * their existing behaviour; the texture and mask must have identical sizes. */
static qbool Skin_TranslateTFHDPlayer(player_info_t *player, int playernum, qbool teammate)
{
	static const char *names[] = { "tf_scout", "tf_snipe", "tf_sold", "tf_demo",
		"tf_medic", "tf_hwguy", "tf_pyro", "tf_spy", "tf_eng" };
	byte *original, *mask, *pixels;
	int i, mw, mh;
	char path[MAX_OSPATH];
	tf_skin_color_t colors[2];
	if (!cl.teamfortress || gl_no24bit.integer || gl_nocolors.integer || !player->skin) return false;
	for (i = 0; i < 9; ++i) if (!strcasecmp(player->skin->name, names[i])) break;
	if (i == 9) return false;
	original = Skin_Cache(player->skin, false);
	if (!original || player->skin->bpp != 4) return false;
	snprintf(path, sizeof(path), "skins/%s_mask", names[i]);
	mask = R_LoadImagePixels(path, 0, 0, TEX_NO_PCX, &mw, &mh);
	if (!mask) return false;
	if (mw != player->skin->width || mh != player->skin->height || mw < 1 || mh < 1 || mw > 4096 || mh > 4096) {
		Q_free(mask);
		return false;
	}
	memset(colors, 0, sizeof(colors));
	colors[0].enabled = colors[1].enabled = true;
	colors[0].palette = player->topcolor;
	colors[1].palette = player->bottomcolor;
	colors[0].rgb = player->topcolor_rgb;
	colors[1].rgb = player->bottomcolor_rgb;
	memcpy(colors[0].color, player->forced_topcolor_rgb, 3);
	memcpy(colors[1].color, player->forced_bottomcolor_rgb, 3);
	pixels = Q_malloc(mw * mh * 4);
	TF_ModelSkinTranslateRGBA(original, mask, mw * mh, d_8to24table, colors, pixels);
	Com_DPrintf("TF HD player skin: %s (%dx%d)\n", names[i], mw, mh);
	R_BlendPlayerSkin(player->skin, teammate, playernum, pixels, mw, mh, false);
	Q_free(pixels);
	Q_free(mask);
	return true;
}

static byte *Skin_LoadTFHDModel(entity_t *ent, int skin, byte **mask, int *width, int *height)
{
	byte *rgba = NULL;
	char path[MAX_OSPATH], base[64];
	int mw, mh, dir;
	*mask = NULL;
	COM_StripExtension(COM_SkipPath(ent->model->name), base, sizeof(base));
	for (dir = 0; dir < 2; ++dir) {
		snprintf(path, sizeof(path), "%s/%s_%d", dir ? "textures" : "textures/models", base, skin);
		rgba = R_LoadImagePixels(path, 0, 0, TEX_NO_PCX, width, height);
		if (!rgba) continue;
		strlcat(path, "_mask", sizeof(path));
		*mask = R_LoadImagePixels(path, 0, 0, TEX_NO_PCX, &mw, &mh);
		if (*mask && *width > 0 && *height > 0 && *width <= 4096 && *height <= 4096 && mw == *width && mh == *height) {
			Com_DPrintf("TF HD model skin: %s_%d (%dx%d)\n", base, skin, *width, *height);
			return rgba;
		}
		Q_free(rgba);
		Q_free(*mask);
		*mask = NULL;
	}
	return NULL;
}

//Translates a skin texture by the per-player color lookup
void R_TranslatePlayerSkin(int playernum)
{
	byte translate[256], *inrow, *original;
	char s[512];
	int	top, bottom, i, j, scaled_width, scaled_height, inwidth, inheight, tinwidth, tinheight;
	unsigned translate32[256], *out, frac, fracstep;
	unsigned pixels[512 * 256];
	extern byte player_8bit_texels[256 * 256];
	extern cvar_t gl_scaleModelTextures;
	player_info_t *player;
	qbool teammate = false;

	player = &cl.players[playernum];
	if (!player->name[0]) {
		return;
	}

	strlcpy(s, Skin_FindName(player, &teammate), sizeof(s));
	COM_StripExtension(s, s, sizeof(s));

	if (player->skin && strcasecmp(s, player->skin->name)) {
		player->skin = NULL;
	}

	if (player->_topcolor == player->topcolor && player->_bottomcolor == player->bottomcolor &&
		player->_topcolor_rgb == player->topcolor_rgb && player->_bottomcolor_rgb == player->bottomcolor_rgb &&
		(!player->topcolor_rgb || !memcmp(player->_forced_topcolor_rgb, player->forced_topcolor_rgb, 3)) &&
		(!player->bottomcolor_rgb || !memcmp(player->_forced_bottomcolor_rgb, player->forced_bottomcolor_rgb, 3)) &&
		player->skin && player->teammate == player->_teammate) {
		return;
	}

	player->_topcolor = player->topcolor;
	player->_bottomcolor = player->bottomcolor;
	player->_teammate = player->teammate;
	player->_topcolor_rgb = player->topcolor_rgb;
	player->_bottomcolor_rgb = player->bottomcolor_rgb;
	memcpy(player->_forced_topcolor_rgb, player->forced_topcolor_rgb, 3);
	memcpy(player->_forced_bottomcolor_rgb, player->forced_bottomcolor_rgb, 3);

	if (!player->skin) {
		Skin_Find(player);
	}

	Skin_RemoveSkinsForPlayer(playernum);
	if (Skin_TranslateTFHDPlayer(player, playernum, teammate)) return;

	if (R_TextureReferenceIsValid(player->skin->texnum[skin_base]) && player->skin->bpp == 4 && !(r_enemyskincolor.string[0] || r_teamskincolor.string[0])) {
		// do not even bother call Skin_Cache(), we have texture num already
		if (teammate && R_TextureReferenceIsValid(player->skin->texnum[skin_base_teammate])) {
			playerskins[playernum].base = player->skin->texnum[skin_base_teammate];
		}
		else {
			playerskins[playernum].base = player->skin->texnum[skin_base];
		}

		if (teammate && R_TextureReferenceIsValid(player->skin->texnum[skin_dead_teammate])) {
			playerskins[playernum].dead = player->skin->texnum[skin_dead_teammate];
		}
		else {
			playerskins[playernum].dead = player->skin->texnum[skin_dead];
		}
		if (!R_TextureReferenceIsValid(playerskins[playernum].dead)) {
			playerskins[playernum].dead = playerskins[playernum].base;
		}
		return;
	}

	if ((original = Skin_Cache(player->skin, false)) != NULL) {
		switch (player->skin->bpp) {
		case 4:
			// 32 bit skin
			R_BlendPlayerSkin(player->skin, teammate, playernum, original, player->skin->width, player->skin->height, false);
			return;

		case 1:
			break;

		default:
			Sys_Error("R_TranslatePlayerSkin: wrong bpp %d", player->skin->bpp);
		}

		//skin data width
		inwidth = 320;
		inheight = 200;
	}
	else {
		original = player_8bit_texels;
		inwidth = 296;
		inheight = 194;
	}

	// locate the original skin pixels
	// real model width
	tinwidth = 296;
	tinheight = 194;

	top = bound(0, player->topcolor, 13) * 16;
	bottom = bound(0, player->bottomcolor, 13) * 16;

	for (i = 0; i < 256; i++) {
		translate[i] = i;
	}

	for (i = 0; i < 16; i++) {
		// the artists made some backwards ranges.  sigh.
		translate[TOP_RANGE + i] = (top < 128) ? (top + i) : (top + 15 - i);
		translate[BOTTOM_RANGE + i] = (bottom < 128) ? (bottom + i) : (bottom + 15 - i);
	}

	scaled_width = gl_scaleModelTextures.value ? min(gl_max_size.value, tinwidth) : min(glConfig.gl_max_size_default, tinwidth);
	scaled_height = gl_scaleModelTextures.value ? min(gl_max_size.value, tinheight) : min(glConfig.gl_max_size_default, tinheight);

	// allow users to crunch sizes down even more if they want
	scaled_width >>= (int)gl_playermip.value;
	scaled_height >>= (int)gl_playermip.value;
	if (scaled_width < 1) {
		scaled_width = 1;
	}
	if (scaled_height < 1) {
		scaled_height = 1;
	}

	for (i = 0; i < 256; i++) {
		translate32[i] = d_8to24table[translate[i]];
	}
	if (player->topcolor_rgb) {
		Skin_SetRGBTranslationRamp(translate32, TOP_RANGE, player->forced_topcolor_rgb);
	}
	if (player->bottomcolor_rgb) {
		Skin_SetRGBTranslationRamp(translate32, BOTTOM_RANGE, player->forced_bottomcolor_rgb);
	}

	out = pixels;
	memset(pixels, 0, sizeof(pixels));
	fracstep = tinwidth * 0x10000 / scaled_width;
	for (i = 0; i < scaled_height; i++, out += scaled_width) {
		inrow = original + inwidth*(i * tinheight / scaled_height);
		frac = fracstep >> 1;
		for (j = 0; j < scaled_width; j += 4) {
			out[j] = translate32[inrow[frac >> 16]];
			frac += fracstep;
			out[j + 1] = translate32[inrow[frac >> 16]];
			frac += fracstep;
			out[j + 2] = translate32[inrow[frac >> 16]];
			frac += fracstep;
			out[j + 3] = translate32[inrow[frac >> 16]];
			frac += fracstep;
		}
	}

	R_BlendPlayerSkin(player->skin, teammate, playernum, (byte*)pixels, scaled_width, scaled_height, false);

	// TODO: Dead skins

	if (Img_HasFullbrights((byte *)original, inwidth * inheight)) {
		out = pixels;
		memset(pixels, 0, sizeof(pixels));
		fracstep = tinwidth * 0x10000 / scaled_width;

		// make all non-fullbright colors transparent
		for (i = 0; i < scaled_height; i++, out += scaled_width) {
			inrow = original + inwidth * (i * tinheight / scaled_height);
			frac = fracstep >> 1;
			for (j = 0; j < scaled_width; j += 4) {
				if (inrow[frac >> 16] < 224) {
					out[j] = translate32[inrow[frac >> 16]] & LittleLong(0x00FFFFFF); // transparent
				}
				else {
					out[j] = translate32[inrow[frac >> 16]]; // fullbright
				}
				frac += fracstep;
				if (inrow[frac >> 16] < 224) {
					out[j + 1] = translate32[inrow[frac >> 16]] & LittleLong(0x00FFFFFF);
				}
				else {
					out[j + 1] = translate32[inrow[frac >> 16]];
				}
				frac += fracstep;
				if (inrow[frac >> 16] < 224) {
					out[j + 2] = translate32[inrow[frac >> 16]] & LittleLong(0x00FFFFFF);
				}
				else {
					out[j + 2] = translate32[inrow[frac >> 16]];
				}
				frac += fracstep;
				if (inrow[frac >> 16] < 224) {
					out[j + 3] = translate32[inrow[frac >> 16]] & LittleLong(0x00FFFFFF);
				}
				else {
					out[j + 3] = translate32[inrow[frac >> 16]];
				}
				frac += fracstep;
			}
		}

		R_BlendPlayerSkin(player->skin, teammate, playernum, (byte*)pixels, scaled_width, scaled_height, true);
	}
}

/* Keep texture forcing and outlines on the same immutable visual team.
 * Never use the owner's scoreboard colors for a disguise or a corpse. */
qbool R_TFModelAppearance(entity_t* ent, player_info_t* appearance)
{
	static const int team_colors[] = { 0, 13, 4, 12, 11 };
	int team = TF_ModelSkinTeam(ent->model->name);
	if (!cl.teamfortress || !team) return false;
	memset(appearance, 0, sizeof(*appearance));
	appearance->topcolor = appearance->bottomcolor = team_colors[team];
	appearance->teammate = TP_TFTeamTeammate(team);
	if (!gl_nocolors.integer) {
		TP_ApplyForcedColors(appearance, appearance->teammate, team, true);
	}
	return true;
}

qbool R_SetSkinForTFModel(entity_t* ent, texture_ref* texture)
{
	int team = TF_ModelSkinTeam(ent->model->name);
	int skin = ent->skinnum, variant, count, width, height, hd_enabled;
	aliashdr_t *hdr;
	player_info_t appearance;
	tf_skin_color_t colors[2];
	byte *mask, *rgba, *hd_mask;
	unsigned int *pixels;
	qbool teammate;
	char identifier[128];
	if (!team) return false;
	/* Do not fall through to the scoreboard skin, even if forcing is off or
	 * this is an incompatible replacement model. Its baked skin is authoritative. */
	if (!cl.teamfortress || gl_nocolors.integer || !ent->model->cached_data) return true;
	hdr = ent->model->cached_data;
	if (skin < 0 || skin >= hdr->numskins) skin = 0;
	if (!hdr->tf_skinpixels[skin]) return true;
	teammate = TP_TFTeamTeammate(team);
	variant = teammate ? 1 : 0;
	R_TFModelAppearance(ent, &appearance);
	memset(colors, 0, sizeof(colors));
	colors[0].enabled = appearance.topcolor_rgb || (teammate ? cl_teamtopcolor.integer : cl_enemytopcolor.integer) != -1;
	colors[1].enabled = appearance.bottomcolor_rgb || (teammate ? cl_teambottomcolor.integer : cl_enemybottomcolor.integer) != -1;
	if (!colors[0].enabled && !colors[1].enabled) return true;
	colors[0].palette = appearance.topcolor;
	colors[1].palette = appearance.bottomcolor;
	colors[0].rgb = appearance.topcolor_rgb;
	colors[1].rgb = appearance.bottomcolor_rgb;
	memcpy(colors[0].color, appearance.forced_topcolor_rgb, 3);
	memcpy(colors[1].color, appearance.forced_bottomcolor_rgb, 3);
	hd_enabled = !gl_no24bit.integer && !RuleSets_DisallowExternalTexture(ent->model);
	if (!R_TextureReferenceIsValid(hdr->tf_textures[skin][variant]) ||
		hdr->tf_hd_enabled[skin][variant] != hd_enabled || memcmp(colors, hdr->tf_colors[skin][variant], sizeof(colors))) {
		width = hdr->skinwidth;
		height = hdr->skinheight;
		rgba = hd_enabled ? Skin_LoadTFHDModel(ent, skin, &hd_mask, &width, &height) : NULL;
		if (!rgba) { width = hdr->skinwidth; height = hdr->skinheight; hd_mask = NULL; }
		count = hdr->skinwidth * hdr->skinheight;
		mask = Q_malloc(count);
		if (!TF_ModelSkinMask(TF_ModelSkinKind(ent->model->name), skin, hdr->skinwidth, hdr->skinheight, mask)) {
			Q_free(mask);
			Q_free(rgba);
			Q_free(hd_mask);
			return true;
		}
		pixels = Q_malloc(width * height * sizeof(*pixels));
		if (rgba) TF_ModelSkinTranslateRGBA(rgba, hd_mask, width * height, d_8to24table, colors, (byte *)pixels);
		else TF_ModelSkinTranslate((byte *)hdr + hdr->tf_skinpixels[skin], mask, count, d_8to24table, colors, pixels);
		snprintf(identifier, sizeof(identifier), "$tf-%s-%d-%d", ent->model->name, skin, variant);
		/* Modern renderer uses alpha zero for fullbright pixels in merged skins. */
		if (!rgba && R_CompressFullbrightTextures()) {
			int i;
			byte *original = (byte *)hdr + hdr->tf_skinpixels[skin];
			for (i = 0; i < count; ++i) if (original[i] >= 224 && !mask[i]) ((byte *)&pixels[i])[3] = 0;
		}
		hdr->tf_textures[skin][variant] = R_LoadTexture(identifier, width, height,
			(byte *)pixels, TEX_MIPMAP | TEX_NOSCALE | (R_CompressFullbrightTextures() ? TEX_ALPHA | TEX_MERGED_LUMA : 0), 4);
		memcpy(hdr->tf_colors[skin][variant], colors, sizeof(colors));
		hdr->tf_hd_enabled[skin][variant] = hd_enabled;
		Q_free(rgba);
		Q_free(hd_mask);
		Q_free(pixels);
		Q_free(mask);
	}
	*texture = hdr->tf_textures[skin][variant];
	return true;
}

void R_SetSkinForPlayerEntity(entity_t* ent, texture_ref* texture, texture_ref* fb_texture, byte** color32bit)
{
	qbool is_player_model = (ent->model->modhint == MOD_PLAYER || ent->renderfx & RF_PLAYERMODEL);
	int playernum = ent->scoreboard - cl.players;

	if (!gl_nocolors.integer) {
		// we can't dynamically colormap textures, so they are cached separately for the players.  Heads are just uncolored.
		if (!ent->scoreboard->skin) {
			CL_NewTranslation(playernum);
		}

		*fb_texture = playerskins[playernum].fb;
		if (ISDEAD(ent->frame) && r_skincolormodedead.integer != -1 && r_skincolormodedead.integer != r_skincolormode.integer) {
			if (r_skincolormodedead.integer && R_TextureReferenceIsValid(playerskins[playernum].dead)) {
				*texture = playerskins[playernum].dead;
			}
		}
		else {
			*texture = playerskins[playernum].base;
		}

		if (is_player_model && R_TextureReferenceEqual(*texture, solidwhite_texture)) {
			qbool custom_skins_blocked = cl.teamfortress || (cl.fpd & (FPD_NO_FORCE_SKIN | FPD_NO_FORCE_COLOR));
			if (!custom_skins_blocked) {
				cvar_t* cv = &r_enemyskincolor;
				if (cl.teamplay && !strcmp(cl.players[playernum].team, TP_SkinForcingTeam())) {
					cv = &r_teamskincolor;
				}
				if (cv->string[0]) {
					*color32bit = cv->color;
				}
			}
		}
	}
}

void Skin_InvalidateTextures(void)
{
	memset(playerskins, 0, sizeof(playerskins));
}

void Skin_RegisterCvars(void)
{
	Cvar_SetCurrentGroup(CVAR_GROUP_SKIN);
	Cvar_Register(&noskins);
	Cvar_Register(&baseskin);
	Cvar_Register(&cl_name_as_skin);
	Cvar_Register(&enemyforceskins);
	Cvar_Register(&teamforceskins);
	Cvar_Register(&r_fullbrightSkins);
	Cvar_Register(&r_enemyskincolor);
	Cvar_Register(&r_teamskincolor);
	Cvar_Register(&r_skincolormode);
	Cvar_Register(&r_skincolormodedead);
}

// Called when connected to server and skin-forcing rules have changed
void Skin_Refresh(void)
{
	int oldskins = noskins.integer;
	int oldflags = noskins.flags;

	noskins.flags &= ~(CVAR_RELOAD_GFX);
	Cvar_SetValue(&noskins, 2);

	con_suppress = true;
	Skin_Skins_f();
	con_suppress = false;

	Cvar_SetValue(&noskins, oldskins);
	noskins.flags = oldflags;
}

void Skin_UserInfoChange(int slot, player_info_t* player, const char* key)
{
	qbool update_skin;
	int mynum;

	if (!cl.spectator || (mynum = Cam_TrackNum()) == -1) {
		mynum = cl.playernum;
	}

	update_skin = !key || (!player->spectator && (!strcmp(key, "skin") || !strcmp(key, "topcolor") ||
		!strcmp(key, "bottomcolor") || !strcmp(key, "team") ||
		(!strcmp(key, "name") && cl_name_as_skin.value)));

	if (slot == mynum && TP_NeedRefreshSkins() && strcmp(player->team, player->_team)) {
		TP_RefreshSkins();
	}
	else if (update_skin) {
		TP_RefreshSkin(slot);
	}
}

void OnChangeSkinForcing(cvar_t *var, char *string, qbool *cancel)
{
	extern cvar_t cl_name_as_skin, enemyforceskins;

	if (cl.teamfortress || (cl.fpd & FPD_NO_FORCE_SKIN)) {
		return;
	}

	if (var == &cl_name_as_skin && (!cls.demoplayback && !cl.spectator)) {
		return; // allow in demos or for specs
	}

	if (var == &enemyforceskins && (!cl.spectator && cls.state != ca_disconnected)) {
		if (!cl.standby && !cl.countdown) {
			Con_Printf("%s cannot be changed during match\n", var->name);
			return;
		}

		if (Q_atoi(string)) {
			Cbuf_AddText("say Individual enemy skins: enabled\n");
		}
		else if (strcmp(var->string, string)) {
			Cbuf_AddText("say Individual enemy skins: disabled\n");
		}
	}

	if (cls.state == ca_active) {
		Cvar_Set(var, string);

		Skin_Refresh();
		*cancel = true;
		return;
	}
}
