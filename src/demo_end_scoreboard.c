#include "demo_end_scoreboard.h"

#include <ctype.h>
#include <stddef.h>

static int Demo_StringStartsWithNoCase(const char *text, const char *prefix)
{
	if (!text || !prefix)
		return 0;

	while (*prefix) {
		if (!*text || tolower((unsigned char)*text) != tolower((unsigned char)*prefix))
			return 0;
		++text;
		++prefix;
	}
	return 1;
}

int Demo_EndScoreboardShouldHold(const demo_end_scoreboard_context_t *context)
{
	return context && context->enabled
		&& (context->local_mvd || context->qtv_replay)
		&& context->teamfortress && context->teamplay
		&& !context->timedemo && !context->playlist
		&& !context->capturing && !context->startup_demo;
}

int Demo_QTVSourceIsReplay(const char *source)
{
	return Demo_StringStartsWithNoCase(source, "demo:")
		|| Demo_StringStartsWithNoCase(source, "file:")
		|| Demo_StringStartsWithNoCase(source, "dir:");
}
