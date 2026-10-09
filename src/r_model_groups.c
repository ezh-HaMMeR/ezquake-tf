#include "quakedef.h"
#include "gl_model.h"
#include "rulesets.h"
#include "r_model_groups.h"

extern cvar_t gl_fb_tfmodels;
static cvar_t gl_fb_tfmodels_filter = {"gl_fb_tfmodels_filter", TF_FB_DEFAULT_FILTER};
#define OUTLINE_SLOT(n) { {"gl_outline_group" #n, ""}, {"gl_outline_group" #n "_color", "0 0 0", CVAR_COLOR}, {"gl_outline_group" #n "_scale", "1"}, {"gl_outline_group" #n "_enable", "1"} }
static struct { cvar_t groups, color, scale, enable; } outlines[] = {
    OUTLINE_SLOT(1), OUTLINE_SLOT(2), OUTLINE_SLOT(3), OUTLINE_SLOT(4)
};

void R_ModelGroupsInit(void)
{
    size_t i;
    Cvar_Register(&gl_fb_tfmodels_filter);
    for (i=0; i<sizeof(outlines)/sizeof(outlines[0]); ++i) {
        Cvar_Register(&outlines[i].groups);
        Cvar_Register(&outlines[i].color);
        Cvar_Register(&outlines[i].scale);
        Cvar_Register(&outlines[i].enable);
    }
}

tf_model_group_t R_EntityModelGroup(const entity_t *ent)
{
    tf_model_group_t group;
    model_t *m = ent->model;
    if (!m || (ent->renderfx & (RF_WEAPONMODEL | RF_VWEPMODEL)) ||
        (m->type != mod_alias && m->type != mod_alias3)) return TF_GROUP_NONE;
    if (m->modhint == MOD_VMODEL || m->modhint == MOD_EYES ||
        m->modhint == MOD_THUNDERBOLT || m->modhint == MOD_FLAME ||
        m->modhint == MOD_FLAME0 || m->modhint == MOD_FLAME2 || m->modhint == MOD_FLAME3 ||
        m->modhint == MOD_LASER || m->modhint == MOD_LAVABALL ||
        m->modhint == MOD_TELEPORTDESTINATION || TF_ModelNameExcluded(m->name)) return TF_GROUP_NONE;
    if ((ent->renderfx & RF_PLAYERMODEL) || m->modhint == MOD_PLAYER) return TF_GROUP_PLAYERS;
    group = TF_ModelNameGroup(m->name);
    if (group != TF_GROUP_NONE) return group;
    switch (m->modhint) {
    case MOD_GIB: return TF_GROUP_PLAYERS;
    case MOD_GRENADE: case MOD_DETPACK: return TF_GROUP_GRENADES;
    case MOD_SENTRYGUN: case MOD_TESLA: case MOD_BUILDINGGIBS: return TF_GROUP_BUILDINGS;
    case MOD_ROCKET: case MOD_CLUSTER: case MOD_SPIKE: return TF_GROUP_PROJECTILES;
    case MOD_FLAG: return TF_GROUP_OBJECTIVES;
    case MOD_BACKPACK: case MOD_ARMOR: case MOD_QUAD: case MOD_PENT: case MOD_RING:
    case MOD_SUIT: case MOD_ROCKETLAUNCHER: case MOD_GRENADELAUNCHER: case MOD_LIGHTNINGGUN:
    case MOD_MEGAHEALTH: return TF_GROUP_PICKUPS;
    default: return TF_GROUP_PROPS;
    }
}

qbool R_ModelUsesMapLighting(const entity_t *ent)
{
    return TF_GroupUsesMapLighting(R_EntityModelGroup(ent), cl.teamfortress,
        gl_fb_tfmodels.integer, TF_ModelGroupMask(gl_fb_tfmodels_filter.string));
}

qbool R_SetEntityOutlineStyle(entity_t *ent)
{
    size_t i;
    tf_model_group_t group = R_EntityModelGroup(ent);
    /* Negative scale preserves the renderer's existing global style. */
    ent->outlineStyle[0] = ent->outlineStyle[1] = ent->outlineStyle[2] = 0;
    ent->outlineStyle[3] = -1;
    for (i=0; i<sizeof(outlines)/sizeof(outlines[0]); ++i) {
        if (TF_ModelGroupMask(outlines[i].groups.string) & group) {
            int c;
            ent->outlineStyle[3] = RuleSets_ClampModelOutlineScale(outlines[i].scale.value);
            for (c=0; c<3; ++c) ent->outlineStyle[c] = outlines[i].color.color[c] / 255.0f;
            return outlines[i].enable.integer && ent->outlineStyle[3] > 0;
        }
    }
    return true;
}
