#ifndef EZQUAKE_TF_MODEL_LIGHTING_H
#define EZQUAKE_TF_MODEL_LIGHTING_H

#include <stddef.h>
#include <string.h>

/* Logical model names, not archive provenance: MDL and MD3 replacements agree. */
static int TF_UseMapLighting(const char *name, int teamfortress, int fullbright)
{
    static const char *const models[] = {
        "turrgun", "turrbase", "disp", "detpack", "coil", "tesla",
        "tgib1", "tgib2", "tgib3", "dgib", "dgib1", "dgib2", "dgib3",
        "tesgib1", "tesgib2", "tesgib3", "tesgib4",
        "grenade", "hgren2", "biggren", "grenade2", "grenade3",
        "caltrop", "flare", "spike"
    };
    size_t i;
    if (!teamfortress || fullbright || !name || strncmp(name, "progs/", 6)) {
        return 0;
    }
    name += 6;
    for (i = 0; i < sizeof(models) / sizeof(models[0]); ++i) {
        size_t length = strlen(models[i]);
        if (!strncmp(name, models[i], length)
            && (!strcmp(name + length, ".mdl") || !strcmp(name + length, ".md3"))) {
            return 1;
        }
    }
    /* Flames, muzzle flashes, players and weapon viewmodels retain their policy. */
    return 0;
}

#endif
