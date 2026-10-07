/* Opt-in MD3 rotor animation. Frames 0..9 retain the TF server protocol. */
#ifndef EZ_TF_SENTRY_ANIMATION_H
#define EZ_TF_SENTRY_ANIMATION_H
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define TF_SENTRY_SPIN_FIRST 10
#define TF_SENTRY_SPIN_POSES 24
#define TF_SENTRY_SPIN_LEVELS 3
#define TF_SENTRY_SPIN_FRAMES (TF_SENTRY_SPIN_FIRST + TF_SENTRY_SPIN_POSES * TF_SENTRY_SPIN_LEVELS)

#define TF_SENTRY_RIG_FRAMES 13
static int TF_SentryRigFramesValid(const char *names, int count, size_t stride)
{
    int level;
    if (count != TF_SENTRY_RIG_FRAMES || stride < 16) return 0;
    for (level = 0; level < 3; ++level) {
        char expected[16] = {0};
        snprintf(expected, sizeof(expected), "tfrig%d_x", level + 1);
        if (memcmp(names + (10 + level) * stride, expected, 16)) return 0;
    }
    return 1;
}

static int TF_SentrySpinNextFrame(int frame)
{
    int local = frame - TF_SENTRY_SPIN_FIRST;
    if (local < 0 || local >= TF_SENTRY_SPIN_POSES * TF_SENTRY_SPIN_LEVELS) return -1;
    return TF_SENTRY_SPIN_FIRST + (local / TF_SENTRY_SPIN_POSES) * TF_SENTRY_SPIN_POSES
        + (local + 1) % TF_SENTRY_SPIN_POSES;
}

static int TF_SentrySpinFramesValid(const char *names, int count, size_t stride)
{
    int level, pose;
    if (count != TF_SENTRY_SPIN_FRAMES || stride < 16) return 0;
    for (level = 0; level < TF_SENTRY_SPIN_LEVELS; ++level) {
        for (pose = 0; pose < TF_SENTRY_SPIN_POSES; ++pose) {
            char expected[16] = { 0 };
            snprintf(expected, sizeof(expected), "tfspin%d_%02d", level + 1, pose);
            if (memcmp(names + (TF_SENTRY_SPIN_FIRST + level * TF_SENTRY_SPIN_POSES + pose) * stride, expected, 16)) return 0;
        }
    }
    return 1;
}

static int TF_SentrySpinPoses(int frame, int oldframe, float network_lerp, double time,
    int *first, int *second, float *fraction)
{
    int level, base, pose, active;
    double phase;
    if (frame < 0 || frame >= 9 || !isfinite(time) || time < 0) return 0;
    level = frame / 3;
    active = frame % 3 != 0;
    /* The idle pose also occurs inside the firing cycle. Bridge its 100 ms
       interval, but stop when it persists, on entity reset, or on a level change. */
    if (!active && oldframe >= 0 && oldframe < 9 && oldframe / 3 == level && oldframe % 3 != 0
        && network_lerp >= 0 && network_lerp < 1.25f) active = 1;
    if (!active) return 0;
    /* Two revolutions per game second; demo pause/speed/seek use game time. */
    phase = fmod(time, 0.5) * (TF_SENTRY_SPIN_POSES * 2.0);
    pose = (int)phase;
    base = TF_SENTRY_SPIN_FIRST + level * TF_SENTRY_SPIN_POSES;
    *first = base + pose;
    *second = TF_SentrySpinNextFrame(*first);
    *fraction = (float)(phase - pose);
    return 1;
}
#endif
