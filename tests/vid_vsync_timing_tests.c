#include "vid_vsync_timing.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <math.h>
#include <stdio.h>

int main(void)
{
	vsync_timings_t history;
	double durations[] = { .00020, .00150, .00025, .00022, .00021 };
	int i;
	VSyncTimings_Reset(&history);
	/* Ending without a started frame must not sample an arbitrary timestamp. */
	VSyncTimings_EndFrame(&history, 100);
	assert(history.count == 0);
	for (i = 0; i < 5; ++i) {
		assert(VSyncTimings_Max(&history) == 0);
		VSyncTimings_StartFrame(&history, 10.0 + i);
		VSyncTimings_EndFrame(&history, 10.0 + i + durations[i]);
		/* Duplicate completion notifications and wait-loop reads cannot fill the ring. */
		VSyncTimings_EndFrame(&history, 11.0 + i);
		assert(history.count == i + 1);
	}
	assert(fabs(VSyncTimings_Max(&history) - .0015) < 1e-10);
	for (i = 0; i < 2; ++i) {
		VSyncTimings_StartFrame(&history, 20.0 + i);
		VSyncTimings_EndFrame(&history, 20.0001 + i);
	}
	/* The outlier ages out only after five subsequent completed frames. */
	assert(fabs(VSyncTimings_Max(&history) - .00025) < 1e-10);
	VSyncTimings_Reset(&history);
	assert(history.count == 0 && VSyncTimings_Max(&history) == 0);
	VSyncTimings_StartFrame(&history, 30);
	VSyncTimings_EndFrame(&history, 29);
	assert(history.count == 0);
	VSyncTimings_EndFrame(&history, 31);
	assert(history.count == 0);
	puts("VSync timing regression tests passed");
	return 0;
}
