#ifndef EZQUAKE_TF_SCOREBOARD_H
#define EZQUAKE_TF_SCOREBOARD_H

#include <stddef.h>

const char *TF_ScoreboardClassName(int playerclass);
void TF_ScoreboardFormatClass(char *buffer, size_t buffer_size, int playerclass);
void TF_ScoreboardFormatFooter(char *buffer, size_t buffer_size, const char *mapname,
	int players, int maxplayers, int round_seconds);

#endif // EZQUAKE_TF_SCOREBOARD_H
