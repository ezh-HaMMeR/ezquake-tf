#include "update_common.h"

#include <ctype.h>
#include <string.h>

static unsigned long Update_NextVersionPart(const char **cursor)
{
	unsigned long value = 0;
	const char *p = *cursor;

	while (*p && !isdigit((unsigned char)*p))
		++p;
	while (isdigit((unsigned char)*p)) {
		value = value * 10 + (unsigned long)(*p - '0');
		++p;
	}
	*cursor = p;
	return value;
}

int Update_CompareVersions(const char *left, const char *right)
{
	const char *a = left ? left : "";
	const char *b = right ? right : "";
	int i;

	for (i = 0; i < 4; ++i) {
		unsigned long av = Update_NextVersionPart(&a);
		unsigned long bv = Update_NextVersionPart(&b);
		if (av < bv)
			return -1;
		if (av > bv)
			return 1;
	}
	return 0;
}

static int Update_HasTraversal(const char *path)
{
	const char *p = path;
	while (*p) {
		const char *start = p;
		while (*p && *p != '/' && *p != '\\')
			++p;
		if (p - start == 2 && start[0] == '.' && start[1] == '.')
			return 1;
		if (*p)
			++p;
	}
	return 0;
}

int Update_IsSafeManagedPath(const char *path)
{
	size_t length;

	if (!path || !path[0] || path[0] == '/' || path[0] == '\\' || strchr(path, ':'))
		return 0;
	if (Update_HasTraversal(path))
		return 0;

	length = strlen(path);
	if (!strcmp(path, "ezquake.exe") || !strcmp(path, "update.exe"))
		return 1;
	if (!strncmp(path, "qw/config_editor/", 17) && length > 22 &&
		!strcmp(path + length - 5, ".json"))
		return 1;
	if (!strncmp(path, "qw/gfx/", 7) && length > 11 &&
		!strcmp(path + length - 4, ".png"))
		return 1;
	if (!strncmp(path, "qw/textures/", 12) && length > 16 &&
		!strcmp(path + length - 4, ".png"))
		return 1;
	return 0;
}
