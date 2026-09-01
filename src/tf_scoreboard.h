#ifndef EZQUAKE_TF_SCOREBOARD_H
#define EZQUAKE_TF_SCOREBOARD_H

#include <stddef.h>

const char *TF_ScoreboardClassName(int playerclass);
int TF_ScoreboardShouldShowClass(int viewer_is_spectator, int viewer_team,
	int target_is_spectator, int target_team);
int TF_ClockDisplaySeconds(int elapsed_seconds, int timelimit_minutes, int countdown);
int TF_MatchClockDisplaySeconds(int elapsed_seconds, int timelimit_minutes, int countdown,
	int prematch, int server_time_ms, int prematch_end_ms, int match_end_ms);
void TF_ScoreboardFormatClass(char *buffer, size_t buffer_size, int playerclass);
void TF_ScoreboardFormatFooter(char *buffer, size_t buffer_size, const char *mapname,
	int players, int maxplayers, int round_seconds);

#endif // EZQUAKE_TF_SCOREBOARD_H
