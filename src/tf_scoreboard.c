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

int TF_ClockDisplaySeconds(int elapsed_seconds, int timelimit_minutes, int countdown)
{
	long long limit_seconds;

	if (elapsed_seconds < 0)
		elapsed_seconds = 0;
	limit_seconds = (long long)timelimit_minutes * 60;
	if (!countdown || limit_seconds <= 0)
		return elapsed_seconds;
	if (elapsed_seconds >= limit_seconds)
		return 0;
	return (int)(limit_seconds - elapsed_seconds);
}

int TF_MatchClockDisplaySeconds(int elapsed_seconds, int timelimit_minutes, int countdown,
	int prematch, int server_time_ms, int prematch_end_ms, int match_end_ms)
{
	long long delta_ms;
	long long round_duration_ms;
	int seconds;

	/* New TF servers publish absolute server-time deadlines.  Keeping this
	 * optional preserves the old clock behavior for old servers and demos. */
	if (prematch && server_time_ms >= 0 && prematch_end_ms > 0) {
		delta_ms = (long long)prematch_end_ms - server_time_ms;
		return delta_ms > 0 ? (int)((delta_ms + 999) / 1000) : 0;
	}

	if (server_time_ms >= 0 && prematch_end_ms > 0 && match_end_ms > prematch_end_ms) {
		round_duration_ms = (long long)match_end_ms - prematch_end_ms;
		if (countdown) {
			delta_ms = (long long)match_end_ms - server_time_ms;
			if (delta_ms <= 0)
				return 0;
			if (delta_ms > round_duration_ms)
				delta_ms = round_duration_ms;
			return (int)((delta_ms + 999) / 1000);
		}

		delta_ms = (long long)server_time_ms - prematch_end_ms;
		if (delta_ms < 0)
			delta_ms = 0;
		if (delta_ms > round_duration_ms)
			delta_ms = round_duration_ms;
		seconds = (int)(delta_ms / 1000);
		return seconds;
	}

	return TF_ClockDisplaySeconds(elapsed_seconds, timelimit_minutes, countdown);
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
