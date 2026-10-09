#ifndef EZQUAKE_TF_MODEL_LIGHTING_H
#define EZQUAKE_TF_MODEL_LIGHTING_H
#include <stddef.h>
#include <string.h>
#include <ctype.h>

typedef enum {
    TF_GROUP_NONE = 0, TF_GROUP_PLAYERS = 1, TF_GROUP_GRENADES = 2,
    TF_GROUP_BUILDINGS = 4, TF_GROUP_PROJECTILES = 8, TF_GROUP_PICKUPS = 16,
    TF_GROUP_OBJECTIVES = 32, TF_GROUP_PROPS = 64
} tf_model_group_t;
#define TF_GROUP_ALL 127
#define TF_FB_DEFAULT_FILTER "players pickups objectives props"

/* Tokens are exact, case-insensitive names separated by spaces or commas. */
static unsigned TF_ModelGroupMask(const char *text)
{
    static const char *names[] = {"players", "grenades", "buildings", "projectiles", "pickups", "objectives", "props"};
    unsigned mask = 0;
    if (!text) return 0;
    while (*text) {
        const char *start;
        size_t n, i, j;
        while (*text && (isspace((unsigned char)*text) || *text == ',')) ++text;
        start = text;
        while (*text && !isspace((unsigned char)*text) && *text != ',') ++text;
        n = text - start;
        for (i = 0; i < sizeof(names)/sizeof(names[0]); ++i) {
            if (strlen(names[i]) != n) continue;
            for (j = 0; j < n && tolower((unsigned char)start[j]) == names[i][j]; ++j) {}
            if (j == n) mask |= 1u << i;
        }
    }
    return mask;
}

static int TF_ModelStem(const char *name, const char *stem)
{
    size_t n = strlen(stem);
    return !strncmp(name, stem, n) && (!strcmp(name+n, ".mdl") || !strcmp(name+n, ".md3"));
}

/* Direct .md3 names must keep the same exclusions as logical .mdl names. */
static int TF_ModelNameExcluded(const char *name)
{
    static const char *effects[] = {"eyes", "flame", "flame0", "flame2", "flame3", "bolt", "bolt2", "bolt3", "laser", "lavaball"};
    size_t i;
    if (!name || strncmp(name, "progs/", 6)) return 0;
    name += 6;
    if (!strncmp(name, "v_", 2)) return 1;
    for (i=0; i<sizeof(effects)/sizeof(effects[0]); ++i)
        if (TF_ModelStem(name, effects[i])) return 1;
    return 0;
}

/* Explicit names take priority over fallback hints; never classify by PAK. */
static tf_model_group_t TF_ModelNameGroup(const char *name)
{
    static const struct { const char *stem; tf_model_group_t group; } known[] = {
        {"player",TF_GROUP_PLAYERS},{"headless",TF_GROUP_PLAYERS},{"h_player",TF_GROUP_PLAYERS},{"gib1",TF_GROUP_PLAYERS},{"gib2",TF_GROUP_PLAYERS},{"gib3",TF_GROUP_PLAYERS},
        {"turrgun",TF_GROUP_BUILDINGS},{"turrbase",TF_GROUP_BUILDINGS},{"disp",TF_GROUP_BUILDINGS},{"coil",TF_GROUP_BUILDINGS},{"tesla",TF_GROUP_BUILDINGS},
        {"tgib1",TF_GROUP_BUILDINGS},{"tgib2",TF_GROUP_BUILDINGS},{"tgib3",TF_GROUP_BUILDINGS},{"dgib",TF_GROUP_BUILDINGS},{"dgib1",TF_GROUP_BUILDINGS},{"dgib2",TF_GROUP_BUILDINGS},{"dgib3",TF_GROUP_BUILDINGS},
        {"tesgib1",TF_GROUP_BUILDINGS},{"tesgib2",TF_GROUP_BUILDINGS},{"tesgib3",TF_GROUP_BUILDINGS},{"tesgib4",TF_GROUP_BUILDINGS},
        {"detpack",TF_GROUP_GRENADES},{"detpack2",TF_GROUP_GRENADES},{"grenade",TF_GROUP_GRENADES},{"hgren2",TF_GROUP_GRENADES},{"biggren",TF_GROUP_GRENADES},{"grenade2",TF_GROUP_GRENADES},{"grenade3",TF_GROUP_GRENADES},{"caltrop",TF_GROUP_GRENADES},{"flare",TF_GROUP_GRENADES},
        {"hook",TF_GROUP_PROJECTILES},{"spike",TF_GROUP_PROJECTILES},{"s_spike",TF_GROUP_PROJECTILES},{"amf_spike",TF_GROUP_PROJECTILES},{"missile",TF_GROUP_PROJECTILES},{"minimissile",TF_GROUP_PROJECTILES},
        {"backpack",TF_GROUP_PICKUPS},{"ammobox",TF_GROUP_PICKUPS},{"grenbox",TF_GROUP_PICKUPS},{"medpack",TF_GROUP_PICKUPS},{"armor",TF_GROUP_PICKUPS},{"quaddama",TF_GROUP_PICKUPS},{"invulner",TF_GROUP_PICKUPS},{"invisibl",TF_GROUP_PICKUPS},{"suit",TF_GROUP_PICKUPS},
        {"g_axe",TF_GROUP_PICKUPS},{"g_shot",TF_GROUP_PICKUPS},{"g_shot2",TF_GROUP_PICKUPS},{"g_nail",TF_GROUP_PICKUPS},{"g_nail2",TF_GROUP_PICKUPS},{"g_rock",TF_GROUP_PICKUPS},{"g_rock2",TF_GROUP_PICKUPS},{"g_light",TF_GROUP_PICKUPS},
        {"flag",TF_GROUP_OBJECTIVES},{"tf_flag",TF_GROUP_OBJECTIVES},{"tf_stan",TF_GROUP_OBJECTIVES},{"kkr",TF_GROUP_OBJECTIVES},{"kkb",TF_GROUP_OBJECTIVES},
        {"w_g_key",TF_GROUP_OBJECTIVES},{"w_s_key",TF_GROUP_OBJECTIVES},{"b_g_key",TF_GROUP_OBJECTIVES},{"b_s_key",TF_GROUP_OBJECTIVES},{"stag",TF_GROUP_OBJECTIVES},
        {"ff_flag",TF_GROUP_OBJECTIVES},{"harbflag",TF_GROUP_OBJECTIVES},{"princess",TF_GROUP_OBJECTIVES}
    };
    size_t i;
    if (!name || strncmp(name,"progs/",6)) return TF_GROUP_NONE;
    name += 6;
    for (i=0; i<sizeof(known)/sizeof(known[0]); ++i)
        if (TF_ModelStem(name,known[i].stem)) return known[i].group;
    for (i=1; i<=4; ++i) {
        char stem[] = "tfheadless1";
        stem[10] = '0' + (char)i;
        if (TF_ModelStem(name,stem)) return TF_GROUP_PLAYERS;
        { char body[] = "tfbody1", head[] = "tfhead1";
          body[6] = head[6] = '0' + (char)i;
          if (TF_ModelStem(name,body) || TF_ModelStem(name,head)) return TF_GROUP_PLAYERS; }
    }
    return TF_GROUP_NONE;
}

static int TF_GroupUsesMapLighting(tf_model_group_t group, int teamfortress, int fullbright, unsigned filter)
{
    return teamfortress && group != TF_GROUP_NONE && !((fullbright != 0) ^ ((filter & group) != 0));
}
#endif
