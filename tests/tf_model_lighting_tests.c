#include "tf_model_lighting.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>

int main(void)
{
    const char *lit[] = {
        "progs/turrgun.mdl", "progs/turrbase.md3", "progs/disp.mdl",
        "progs/detpack.md3", "progs/coil.mdl", "progs/tesla.md3",
        "progs/dgib1.md3", "progs/tgib3.mdl", "progs/tesgib4.mdl",
        "progs/grenade.mdl", "progs/hgren2.md3", "progs/biggren.mdl",
        "progs/grenade2.mdl", "progs/grenade3.md3", "progs/flare.mdl",
        "progs/caltrop.mdl", "progs/spike.md3"
    };
    const char *unchanged[] = {
        "progs/player.mdl", "progs/tf_stan.mdl", "progs/tf_sold.mdl",
        "progs/v_rock.mdl", "progs/flame2.mdl", "progs/flame.mdl",
        "progs/missile.mdl", "progs/backpack.mdl", "progs/eyes.mdl",
        "progs/turrgun.mdl.backup", "progs/turrgun", "progs/disp2.md3",
        "other/progs/disp.mdl", "progs/d", "", NULL
    };
    size_t i;
    for (i=0; i<sizeof(lit)/sizeof(lit[0]); ++i) {
        assert(TF_UseMapLighting(lit[i],1,0));
        assert(!TF_UseMapLighting(lit[i],1,1));
        assert(!TF_UseMapLighting(lit[i],1,2));
        assert(!TF_UseMapLighting(lit[i],0,0));
    }
    for (i=0; i<sizeof(unchanged)/sizeof(unchanged[0]); ++i) {
        assert(!TF_UseMapLighting(unchanged[i],1,0));
    }
    puts("TF model lighting scope and opt-out passed");
    return 0;
}
