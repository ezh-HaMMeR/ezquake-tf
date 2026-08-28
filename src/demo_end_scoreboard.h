#ifndef EZQUAKE_DEMO_END_SCOREBOARD_H
#define EZQUAKE_DEMO_END_SCOREBOARD_H

typedef struct demo_end_scoreboard_context_s {
	int enabled;
	int local_mvd;
	int qtv_replay;
	int teamfortress;
	int teamplay;
	int timedemo;
	int playlist;
	int capturing;
	int startup_demo;
} demo_end_scoreboard_context_t;

int Demo_EndScoreboardShouldHold(const demo_end_scoreboard_context_t *context);
int Demo_QTVSourceIsReplay(const char *source);

#endif // EZQUAKE_DEMO_END_SCOREBOARD_H
