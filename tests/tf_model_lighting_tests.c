#include "tf_model_lighting.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>

int main(void)
{
    struct { const char *name; tf_model_group_t group; } cases[] = {
        {"progs/player.mdl", TF_GROUP_PLAYERS}, {"progs/tfheadless4.md3", TF_GROUP_PLAYERS},
        {"progs/tfhead1.mdl", TF_GROUP_PLAYERS}, {"progs/tfbody2.md3", TF_GROUP_PLAYERS},
        {"progs/gib3.mdl", TF_GROUP_PLAYERS}, {"progs/headless.mdl", TF_GROUP_PLAYERS},
        {"progs/turrgun.md3", TF_GROUP_BUILDINGS}, {"progs/turrbase.mdl", TF_GROUP_BUILDINGS},
        {"progs/disp.md3", TF_GROUP_BUILDINGS}, {"progs/dgib1.md3", TF_GROUP_BUILDINGS},
        {"progs/tesgib4.mdl", TF_GROUP_BUILDINGS}, {"progs/coil.mdl", TF_GROUP_BUILDINGS},
        {"progs/grenade.mdl", TF_GROUP_GRENADES}, {"progs/detpack.md3", TF_GROUP_GRENADES},
        {"progs/hgren2.mdl", TF_GROUP_GRENADES}, {"progs/biggren.md3", TF_GROUP_GRENADES},
        {"progs/grenade2.mdl", TF_GROUP_GRENADES}, {"progs/grenade3.md3", TF_GROUP_GRENADES},
        {"progs/caltrop.mdl", TF_GROUP_GRENADES}, {"progs/flare.mdl", TF_GROUP_GRENADES},
        {"progs/spike.md3", TF_GROUP_PROJECTILES}, {"progs/missile.mdl", TF_GROUP_PROJECTILES},
        {"progs/backpack.mdl", TF_GROUP_PICKUPS}, {"progs/g_rock2.md3", TF_GROUP_PICKUPS},
        {"progs/tf_stan.mdl", TF_GROUP_OBJECTIVES}, {"progs/w_g_key.md3", TF_GROUP_OBJECTIVES},
        {"progs/turrgun.mdl.backup", TF_GROUP_NONE}, {"progs/tfheadless5.mdl", TF_GROUP_NONE},
        {"progs/disp", TF_GROUP_NONE}, {"progs/d", TF_GROUP_NONE}, {"", TF_GROUP_NONE}, {NULL, TF_GROUP_NONE}
    };
    unsigned defaults = TF_ModelGroupMask(TF_FB_DEFAULT_FILTER);
    unsigned g, fb, mask;
    size_t i;
    for (i=0; i<sizeof(cases)/sizeof(cases[0]); ++i) assert(TF_ModelNameGroup(cases[i].name)==cases[i].group);
    assert(TF_ModelGroupMask(" PLAYERS, grenades\tBUILDINGS  projectiles pickups objectives props ")==TF_GROUP_ALL);
    assert(TF_ModelGroupMask("players players")==TF_GROUP_PLAYERS);
    assert(TF_ModelGroupMask("player grenades2 all unknown")==0);
    assert(TF_ModelGroupMask(NULL)==0);
    assert(TF_ModelNameExcluded("progs/flame2.md3"));
    assert(TF_ModelNameExcluded("progs/v_rock.mdl"));
    assert(TF_ModelNameExcluded("progs/eyes.md3"));
    assert(!TF_ModelNameExcluded("progs/flare.mdl"));
    assert(!TF_ModelNameExcluded("progs/flame2.md3.backup"));
    for(g=1;g<=TF_GROUP_PROPS;g<<=1) for(fb=0;fb<2;++fb) for(mask=0;mask<=TF_GROUP_ALL;++mask) {
        assert(TF_GroupUsesMapLighting(g,1,fb,mask)==(fb==!!(mask&g)));
        assert(!TF_GroupUsesMapLighting(g,0,fb,mask));
    }
    assert(!TF_GroupUsesMapLighting(TF_GROUP_NONE,1,0,0));
    assert(TF_GroupUsesMapLighting(TF_GROUP_BUILDINGS,1,0,defaults));
    assert(TF_GroupUsesMapLighting(TF_GROUP_GRENADES,1,0,defaults));
    assert(!TF_GroupUsesMapLighting(TF_GROUP_PLAYERS,1,0,defaults));
    assert(!TF_GroupUsesMapLighting(TF_GROUP_PICKUPS,1,0,defaults));
    puts("Model categories, token parsing, defaults and all inversion combinations passed");
    return 0;
}
