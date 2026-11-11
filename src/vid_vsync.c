/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Foobar; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

#include "quakedef.h"
#include "tr_types.h"
#include "vid_vsync_timing.h"

extern cvar_t r_swapInterval;
extern cvar_t vid_vsync_lag_fix;
extern cvar_t vid_vsync_lag_tweak;

static vsync_timings_t timings;
static int timing_frequency;

void VID_ResetVSyncTimings(void)
{
	VSyncTimings_Reset(&timings);
	timing_frequency = 0;
}

qbool VID_VSyncIsOn(void)
{
	return (r_swapInterval.integer != 0);
}

qbool VID_VsyncLagEnabled(void)
{
	return VID_VSyncIsOn() && vid_vsync_lag_fix.integer;
}

void VID_RenderFrameEnd(void)
{
	if (VID_VsyncLagEnabled()) {
		VSyncTimings_EndFrame(&timings, Sys_DoubleTime());
	}
}

// Returns true if it's not time yet to run a frame
qbool VID_VSyncLagFix(void)
{
	extern double vid_last_swap_time;
	double longest_rendertime, now, time_left;

	if (!VID_VSyncIsOn() || !vid_vsync_lag_fix.integer) {
		VID_ResetVSyncTimings();
		return false;
	}

	if (!glConfig.displayFrequency) {
		VID_ResetVSyncTimings();
		Com_Printf("VID_VSyncLagFix: displayFrequency isn't set, can't enable vsync lag fix\n");
		return false;
	}

	if (timing_frequency != glConfig.displayFrequency) {
		VID_ResetVSyncTimings();
		timing_frequency = glConfig.displayFrequency;
	}
	/* Only VID_RenderFrameEnd appends samples: wait-loop iterations are not frames. */
	now = Sys_DoubleTime();
	longest_rendertime = VSyncTimings_Max(&timings);
	if (!longest_rendertime) {
		/* Start timing warmup frames too, without inventing a duration at startup. */
		VSyncTimings_StartFrame(&timings, now);
		return false;
	}
	time_left = vid_last_swap_time + 1.0 / glConfig.displayFrequency - now;
	time_left -= longest_rendertime;
	time_left -= vid_vsync_lag_tweak.value * 0.001;
	if (time_left > 0) {
		extern cvar_t sys_yieldcpu;

		if (time_left > 0.001 && sys_yieldcpu.integer) {
			Sys_MSleep(min(time_left * 1000, 500));
		}

		return true;    // don't run a frame yet
	}

	VSyncTimings_StartFrame(&timings, Sys_DoubleTime());
	return false;
}
