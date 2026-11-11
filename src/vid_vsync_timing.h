#ifndef EZQUAKE_VID_VSYNC_TIMING_H
#define EZQUAKE_VID_VSYNC_TIMING_H

#define VSYNC_TIMING_SAMPLES 5

typedef struct {
	double samples[VSYNC_TIMING_SAMPLES];
	double frame_start;
	int next, count, frame_pending;
} vsync_timings_t;

static inline void VSyncTimings_Reset(vsync_timings_t *history)
{
	int i;
	for (i = 0; i < VSYNC_TIMING_SAMPLES; ++i)
		history->samples[i] = 0;
	history->frame_start = 0;
	history->next = history->count = history->frame_pending = 0;
}

static inline void VSyncTimings_StartFrame(vsync_timings_t *history, double now)
{
	history->frame_start = now;
	history->frame_pending = 1;
}

static inline void VSyncTimings_EndFrame(vsync_timings_t *history, double now)
{
	double duration;
	if (!history->frame_pending)
		return;
	history->frame_pending = 0;
	duration = now - history->frame_start;
	if (duration <= 0)
		return;
	history->samples[history->next] = duration;
	history->next = (history->next + 1) % VSYNC_TIMING_SAMPLES;
	if (history->count < VSYNC_TIMING_SAMPLES)
		++history->count;
}

/* Zero means warmup is incomplete. Waiting iterations never add samples. */
static inline double VSyncTimings_Max(const vsync_timings_t *history)
{
	double longest = 0;
	int i;
	if (history->count < VSYNC_TIMING_SAMPLES)
		return 0;
	for (i = 0; i < VSYNC_TIMING_SAMPLES; ++i) {
		if (history->samples[i] > longest)
			longest = history->samples[i];
	}
	return longest;
}

#endif
