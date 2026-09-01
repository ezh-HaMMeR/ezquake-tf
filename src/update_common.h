#ifndef EZQUAKE_UPDATE_COMMON_H
#define EZQUAKE_UPDATE_COMMON_H

#include <stddef.h>

int Update_CompareVersions(const char *left, const char *right);
int Update_IsSafeManagedPath(const char *path);

#endif
