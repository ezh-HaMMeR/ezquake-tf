/* CPU-side frame diagnostics. No disk I/O or allocations during capture. */
#include <time.h>
#include "quakedef.h"
#include "keys.h"
#include "tr_types.h"
#include "r_framestats.h"
#include "version.h"
#include "frame_capture.h"

#define FRAME_CAPTURE_CAPACITY 1048576

extern qbool ActiveApp, Minimized, physframe;
extern cvar_t r_swapInterval, vid_vsync_lag_fix, vid_vsync_lag_tweak, cl_maxfps;

typedef struct {
	double time_s, interval_ms, work_ms, input_ms, render_ms, swap_ms, tail_ms;
	float x, y, z, yaw, pitch, maxfps, tweak;
	int frame, mouse_x, mouse_y, physics, state, focused, minimized, key_dest;
	int vsync, lag_fix, hz, marker, draw_calls, world_polys, texture_binds;
} frame_capture_row_t;

static frame_capture_row_t *rows;
static frame_capture_row_t current;
static unsigned int row_count;
static FILE *output;
static char output_path[MAX_OSPATH];
static double capture_start, duration, previous_start, frame_start;
static double input_start, render_start, render_end, swap_start;
static qbool in_frame, input_pending, render_pending, swap_pending;
static int marker;

static void FrameCapture_Stop(void)
{
	unsigned int i;
	qbool failed;
	if (!output)
		return;
	/* All formatting and file writes happen after recording stops. */
	fprintf(output, "# CPU-side timings; render_ms includes swap_ms; not GPU or scanout measurements\n");
	fprintf(output, "# release=%s version=%s samples=%u\n", EZQUAKE_TF_RELEASE_VERSION, VERSION, row_count);
	fprintf(output, "frame,time_s,interval_ms,work_ms,input_ms,render_ms,swap_ms,tail_ms,x,y,z,yaw,pitch,mouse_x,mouse_y,physics,state,focused,minimized,key_dest,vsync,lag_fix,tweak_ms,maxfps,hz,marker,draw_calls,world_polys,texture_binds\n");
	for (i = 0; i < row_count; ++i) {
		const frame_capture_row_t *r = &rows[i];
		fprintf(output, "%d,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.3f,%.3f,%.3f,%.4f,%.4f,%d,%d,%d,%d,%d,%d,%d,%d,%d,%.3f,%.1f,%d,%d,%d,%d,%d\n",
			r->frame, r->time_s, r->interval_ms, r->work_ms, r->input_ms, r->render_ms, r->swap_ms, r->tail_ms,
			r->x, r->y, r->z, r->yaw, r->pitch, r->mouse_x, r->mouse_y, r->physics, r->state, r->focused,
			r->minimized, r->key_dest, r->vsync, r->lag_fix, r->tweak, r->maxfps, r->hz, r->marker,
			r->draw_calls, r->world_polys, r->texture_binds);
	}
	failed = ferror(output) != 0;
	if (fclose(output) != 0)
		failed = true;
	output = NULL;
	Q_free(rows);
	in_frame = false;
	Com_Printf("Frame capture %s: %u frames, %s\n", failed ? "WRITE FAILED" : "saved", row_count, output_path);
}

static void FrameCapture_Command(void)
{
	time_t now;
	struct tm local;
	char directory[MAX_OSPATH], stem[48];
	int suffix;
	FILE *existing;
	double seconds = Cmd_Argc() > 1 ? atof(Cmd_Argv(1)) : 120;
	if (Cmd_Argc() == 2 && !strcmp(Cmd_Argv(1), "stop")) {
		FrameCapture_Stop();
		return;
	}
	if (Cmd_Argc() > 2 || !(seconds >= 1 && seconds <= 600)) {
		Com_Printf("Usage: framecapture [seconds: 1..600 | stop]; framecapture_mark marks a suspected stutter\n");
		return;
	}
	if (output) {
		Com_Printf("Already recording; use framecapture stop first\n");
		return;
	}
	snprintf(directory, sizeof(directory), "%s/qw/frametimes", com_basedir);
	Sys_mkdir(directory);
	now = time(NULL);
#ifdef _WIN32
	localtime_s(&local, &now);
#else
	localtime_r(&now, &local);
#endif
	strftime(stem, sizeof(stem), "frames-%Y%m%d-%H%M%S", &local);
	for (suffix = 0; suffix < 1000; ++suffix) {
		snprintf(output_path, sizeof(output_path), "%s/%s-%03d.csv", directory, stem, suffix);
		existing = fopen(output_path, "r");
		if (!existing)
			break;
		fclose(existing);
	}
	if (suffix == 1000 || !(output = fopen(output_path, "w"))) {
		Com_Printf("Cannot create frame capture file in %s\n", directory);
		return;
	}
	rows = Q_calloc(FRAME_CAPTURE_CAPACITY, sizeof(*rows));
	row_count = 0;
	marker = 0;
	duration = seconds;
	previous_start = 0;
	in_frame = false;
	capture_start = Sys_DoubleTime();
	Com_Printf("Recording frames for %.0f seconds (up to %d frames); close console and play normally.\n", seconds, FRAME_CAPTURE_CAPACITY);
	Com_Printf("Output after capture: %s\n", output_path);
}

static void FrameCapture_Mark(void)
{
	if (output)
		++marker;
}

void FrameCapture_Init(void)
{
	Cmd_AddCommand("framecapture", FrameCapture_Command);
	Cmd_AddCommand("framecapture_mark", FrameCapture_Mark);
}

void FrameCapture_Shutdown(void)
{
	FrameCapture_Stop();
}

void FrameCapture_FrameStart(void)
{
	if (!output)
		return;
	frame_start = Sys_DoubleTime();
	if (frame_start - capture_start >= duration || row_count == FRAME_CAPTURE_CAPACITY) {
		FrameCapture_Stop();
		return;
	}
	memset(&current, 0, sizeof(current));
	current.frame = cls.framecount;
	current.time_s = frame_start - capture_start;
	current.interval_ms = previous_start ? (frame_start - previous_start) * 1000 : 0;
	previous_start = frame_start;
	input_pending = render_pending = swap_pending = false;
	render_end = 0;
	in_frame = true;
}

void FrameCapture_FrameEnd(void)
{
	double now;
	if (!in_frame || !output)
		return;
	now = Sys_DoubleTime();
	current.work_ms = (now - frame_start) * 1000;
	current.tail_ms = render_end ? (now - render_end) * 1000 : 0;
	current.x = cl.simorg[0]; current.y = cl.simorg[1]; current.z = cl.simorg[2];
	current.yaw = cl.viewangles[YAW]; current.pitch = cl.viewangles[PITCH];
	current.physics = physframe;
	current.state = cls.state;
	current.focused = ActiveApp;
	current.minimized = Minimized;
	current.key_dest = key_dest;
	current.vsync = r_swapInterval.integer;
	current.lag_fix = vid_vsync_lag_fix.integer;
	current.tweak = vid_vsync_lag_tweak.value;
	current.maxfps = cl_maxfps.value;
	current.hz = glConfig.displayFrequency;
	current.marker = marker;
	current.draw_calls = frameStats.draw_calls;
	current.world_polys = frameStats.classic.polycount[polyTypeWorldModel];
	current.texture_binds = frameStats.texture_binds;
	rows[row_count++] = current;
	in_frame = false;
}

void FrameCapture_InputBegin(void)
{
	if (in_frame) { input_start = Sys_DoubleTime(); input_pending = true; }
}

void FrameCapture_InputEnd(int mouse_x, int mouse_y)
{
	if (in_frame && input_pending) {
		current.input_ms += (Sys_DoubleTime() - input_start) * 1000;
		current.mouse_x += mouse_x; current.mouse_y += mouse_y;
		input_pending = false;
	}
}

void FrameCapture_RenderBegin(void)
{
	if (in_frame) { render_start = Sys_DoubleTime(); render_pending = true; }
}

void FrameCapture_RenderEnd(void)
{
	if (in_frame && render_pending) {
		render_end = Sys_DoubleTime();
		current.render_ms += (render_end - render_start) * 1000;
		render_pending = false;
	}
}

void FrameCapture_SwapBegin(void)
{
	if (in_frame) { swap_start = Sys_DoubleTime(); swap_pending = true; }
}

void FrameCapture_SwapEnd(void)
{
	if (in_frame && swap_pending) {
		current.swap_ms += (Sys_DoubleTime() - swap_start) * 1000;
		swap_pending = false;
	}
}
