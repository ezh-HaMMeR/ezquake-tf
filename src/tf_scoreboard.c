#include "tf_scoreboard.h"

#include <stdio.h>

const char *TF_ScoreboardClassName(int playerclass)
{
	static const char *const names[] = {
		"", "Scout", "Sniper", "Sold", "Demo",
		"Medic", "HWGuy", "Pyro", "Spy", "Eng"
	};

	return playerclass >= 1 && playerclass <= 9 ? names[playerclass] : "";
}

int TF_ScoreboardShouldShowClass(int viewer_is_spectator, int viewer_team,
	int target_is_spectator, int target_team)
{
	return !viewer_is_spectator && !target_is_spectator
		&& viewer_team > 0 && target_team == viewer_team;
}

void TF_ScoreboardFormatClass(char *buffer, size_t buffer_size, int playerclass)
{
	const char *class_name = TF_ScoreboardClassName(playerclass);

	if (!buffer_size)
		return;
	if (!class_name[0]) {
		buffer[0] = 0;
		return;
	}

	snprintf(buffer, buffer_size, "%c%s%c", 0x10, class_name, 0x11);
}

void TF_ScoreboardFormatFooter(char *buffer, size_t buffer_size, const char *mapname,
	int players, int maxplayers, int round_seconds)
{
	int minutes;
	int seconds;

	if (round_seconds < 0)
		round_seconds = 0;
	minutes = (round_seconds / 60) % 60;
	seconds = round_seconds % 60;
	snprintf(buffer, buffer_size, "Map: %s   Players: %d/%d   Round: %02d:%02d",
		mapname && mapname[0] ? mapname : "-", players, maxplayers, minutes, seconds);
}
