#include "tf_scoreboard.h"

#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(condition, message) \
	do { if (!(condition)) { fprintf(stderr, "FAIL: %s\n", message); failures++; } } while (0)

int main(void)
{
	static const char *const expected[] = {
		"", "Scout", "Sniper", "Sold", "Demo", "Medic", "HWGuy", "Pyro", "Spy", "Eng"
	};
	char text[128];
	int i;

	for (i = 1; i <= 9; ++i)
		CHECK(!strcmp(TF_ScoreboardClassName(i), expected[i]), "TF class abbreviation");
	CHECK(!TF_ScoreboardClassName(0)[0] && !TF_ScoreboardClassName(10)[0], "unknown class is blank");
	CHECK(TF_ScoreboardShouldShowClass(0, 2, 0, 2), "teammate class is visible");
	CHECK(!TF_ScoreboardShouldShowClass(0, 2, 0, 1), "enemy class is hidden");
	CHECK(!TF_ScoreboardShouldShowClass(1, 2, 0, 2), "spectator cannot see classes");
	CHECK(!TF_ScoreboardShouldShowClass(0, 0, 0, 0), "unassigned player cannot see classes");
	CHECK(!TF_ScoreboardShouldShowClass(0, 2, 1, 2), "spectator row has no class");
	CHECK(TF_ClockDisplaySeconds(125, 10, 0) == 125, "normal TF clock counts up");
	CHECK(TF_ClockDisplaySeconds(125, 10, 1) == 475, "countdown TF clock uses timelimit");
	CHECK(TF_ClockDisplaySeconds(700, 10, 1) == 0, "countdown TF clock stops at zero");
	CHECK(TF_ClockDisplaySeconds(125, 0, 1) == 125, "countdown without timelimit falls back to elapsed time");
	CHECK(TF_MatchClockDisplaySeconds(0, 16, 1, 1, 12500, 66500, 966500) == 54,
		"prematch clock counts down to the published match start");
	CHECK(TF_MatchClockDisplaySeconds(0, 0, 0, 1, 12500, 66500, 0) == 54,
		"prematch clock does not require a round timelimit");
	CHECK(TF_MatchClockDisplaySeconds(0, 16, 1, 0, 66500, 66500, 966500) == 900,
		"round countdown resets to the actual round duration");
	CHECK(TF_MatchClockDisplaySeconds(15, 16, 1, 0, 81500, 66500, 966500) == 885,
		"round countdown uses the published match deadline");
	CHECK(TF_MatchClockDisplaySeconds(15, 16, 0, 0, 81500, 66500, 966500) == 15,
		"round count-up starts at zero after prematch");
	CHECK(TF_MatchClockDisplaySeconds(15, 16, 1, 0, 81500, 0, 0) == 945,
		"old servers retain the legacy timelimit fallback");

	TF_ScoreboardFormatClass(text, sizeof(text), 6);
	CHECK((unsigned char)text[0] == 0x10 && !strncmp(text + 1, "HWGuy", 5)
		&& (unsigned char)text[6] == 0x11, "class is enclosed by Quake brackets");

	TF_ScoreboardFormatFooter(text, sizeof(text), "2fort5r", 12, 24, 754);
	CHECK(!strcmp(text, "Map: 2fort5r   Players: 12/24   Round: 12:34"), "scoreboard footer");

	if (failures)
		return 1;
	puts("PASS: TF scoreboard class names and footer");
	return 0;
}
