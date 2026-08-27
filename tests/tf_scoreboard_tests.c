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
