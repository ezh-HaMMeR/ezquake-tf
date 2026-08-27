#include "tf_vote.h"

#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(condition, message) \
	do { if (!(condition)) { fprintf(stderr, "FAIL: %s\n", message); failures++; } } while (0)

int main(void)
{
	static const char vote_panel[] =
		"Player votes for changemap\n"
		"--------------------\n"
		"     \x90\x93\x91 Yes\n"
		"     \x90\x94\x91 No\n"
		"     \x90\x92\x91 Close\n"
		"Votes: 1/2\n"
		"Time left: 30 sec\n";
	char output[512];

	CHECK(TF_VoteTransformCenterPrint(vote_panel, output, sizeof(output), 1), "vote panel detected");
	CHECK(strstr(output, "\x90" "F1" "\x91 Yes") != NULL, "yes displays F1");
	CHECK(strstr(output, "\x90" "F2" "\x91 No") != NULL, "no displays F2");
	CHECK(strstr(output, "\x90" "F3" "\x91 Close") != NULL, "close displays F3");
	CHECK(!strcmp(TF_VoteCommandForFunctionKey(1), "cmd vote yes\n"),
		"F1 maps to the vote yes command");
	CHECK(!strcmp(TF_VoteCommandForFunctionKey(2), "cmd vote no\n"),
		"F2 maps to the vote no command");
	CHECK(!strcmp(TF_VoteCommandForFunctionKey(3), "cmd vote close\n"),
		"F3 maps to the vote close command");
	CHECK(TF_VoteCommandForFunctionKey(4) == NULL,
		"other function keys are ignored");

	CHECK(!TF_VoteTransformCenterPrint("ordinary centerprint", output, sizeof(output), 1),
		"ordinary centerprint is not a vote panel");
	CHECK(!strcmp(output, "ordinary centerprint"), "ordinary centerprint remains unchanged");
	TF_VoteTransformCenterPrint(vote_panel, output, sizeof(output), 0);
	CHECK(!strcmp(output, vote_panel), "newvote_f 0 preserves the original panel");

	if (failures)
		return 1;
	puts("PASS: TF vote function-key display and commands");
	return 0;
}
