#ifndef EZQUAKE_R_MODEL_GROUPS_H
#define EZQUAKE_R_MODEL_GROUPS_H
#include "tf_model_lighting.h"
void R_ModelGroupsInit(void);
tf_model_group_t R_EntityModelGroup(const entity_t *ent);
qbool R_ModelUsesMapLighting(const entity_t *ent);
qbool R_SetEntityOutlineStyle(entity_t *ent);
#endif
