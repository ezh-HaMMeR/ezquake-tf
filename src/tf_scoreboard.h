#ifndef EZQUAKE_TF_SCOREBOARD_H
#define EZQUAKE_TF_SCOREBOARD_H

#include <stddef.h>

const char *TF_ScoreboardClassName(int playerclass);
int TF_ScoreboardShouldShowClass(int viewer_is_spectator, int viewer_team,
	int target_is_spectator, int target_team);
void TF_ScoreboardFormatClass(char *buffer, size_t buffer_size, int playerclass);
void TF_ScoreboardFormatFooter(char *buffer, size_t buffer_size, const char *mapname,
	int players, int maxplayers, int round_seconds);

#endif // EZQUAKE_TF_SCOREBOARD_H
