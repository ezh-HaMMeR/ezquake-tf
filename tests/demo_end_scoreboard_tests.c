#include "demo_end_scoreboard.h"

#include <stdio.h>

static int failures;

#define CHECK(condition, message) \
	do { if (!(condition)) { fprintf(stderr, "FAIL: %s\n", message); failures++; } } while (0)

static demo_end_scoreboard_context_t EnabledLocalMVD(void)
{
	demo_end_scoreboard_context_t context = { 0 };
	context.enabled = 1;
	context.local_mvd = 1;
	context.teamfortress = 1;
	context.teamplay = 1;
	return context;
}

int main(void)
{
	demo_end_scoreboard_context_t context = EnabledLocalMVD();

	CHECK(Demo_EndScoreboardShouldHold(&context), "TF teamplay MVD holds the final scoreboard");
	context.local_mvd = 0;
	context.qtv_replay = 1;
	CHECK(Demo_EndScoreboardShouldHold(&context), "recorded demo over QTV holds the final scoreboard");
	context.qtv_replay = 0;
	CHECK(!Demo_EndScoreboardShouldHold(&context), "live QTV is unchanged");

	context = EnabledLocalMVD(); context.enabled = 0;
	CHECK(!Demo_EndScoreboardShouldHold(&context), "cvar disables the behavior");
	context = EnabledLocalMVD(); context.teamfortress = 0;
	CHECK(!Demo_EndScoreboardShouldHold(&context), "non-TF demo is unchanged");
	context = EnabledLocalMVD(); context.teamplay = 0;
	CHECK(!Demo_EndScoreboardShouldHold(&context), "non-teamplay demo is unchanged");
	context = EnabledLocalMVD(); context.timedemo = 1;
	CHECK(!Demo_EndScoreboardShouldHold(&context), "timedemo is unchanged");
	context = EnabledLocalMVD(); context.playlist = 1;
	CHECK(!Demo_EndScoreboardShouldHold(&context), "playlist can advance normally");
	context = EnabledLocalMVD(); context.capturing = 1;
	CHECK(!Demo_EndScoreboardShouldHold(&context), "movie capture can finish normally");
	context = EnabledLocalMVD(); context.startup_demo = 1;
	CHECK(!Demo_EndScoreboardShouldHold(&context), "startup demo is unchanged");

	CHECK(Demo_QTVSourceIsReplay("demo:match.mvd"), "demo source is a replay");
	CHECK(Demo_QTVSourceIsReplay("FILE:match.mvd"), "file source is a replay case-insensitively");
	CHECK(Demo_QTVSourceIsReplay("dir:archive/match.mvd"), "demo directory source is a replay");
	CHECK(!Demo_QTVSourceIsReplay("tcp:server.example:27500"), "TCP source is live");
	CHECK(!Demo_QTVSourceIsReplay("udp:server.example:27500"), "UDP source is live");
	CHECK(!Demo_QTVSourceIsReplay("12@qtv.example:28000"), "ordinary QTV stream is live");
	CHECK(!Demo_QTVSourceIsReplay(NULL), "missing QTV source is not treated as a replay");

	if (failures)
		return 1;
	puts("PASS: demo-end scoreboard policy and QTV source classification");
	return 0;
}
