#include "update_common.h"

#include <stdio.h>

#define CHECK(condition) do { if (!(condition)) { \
	fprintf(stderr, "check failed at line %d: %s\n", __LINE__, #condition); return 1; \
} } while (0)

int main(void)
{
	CHECK(Update_CompareVersions("v2026.08.28", "v2026.08.28") == 0);
	CHECK(Update_CompareVersions("v2026.08.28", "v2026.08.28.1") < 0);
	CHECK(Update_CompareVersions("v2026.09.01", "v2026.08.28.9") > 0);
	CHECK(Update_IsSafeManagedPath("ezquake.exe"));
	CHECK(Update_IsSafeManagedPath("update.exe"));
	CHECK(Update_IsSafeManagedPath("qw/gfx/cursor.png"));
	CHECK(Update_IsSafeManagedPath("qw/textures/icons/sg1.png"));
	CHECK(Update_IsSafeManagedPath("qw/config_editor/dict_settings.json"));
	CHECK(!Update_IsSafeManagedPath("fortress/settings.cfg"));
	CHECK(!Update_IsSafeManagedPath("qw/autoexec.cfg"));
	CHECK(!Update_IsSafeManagedPath("../ezquake.exe"));
	CHECK(!Update_IsSafeManagedPath("qw/textures/../../fortress/settings.cfg"));
	CHECK(!Update_IsSafeManagedPath("C:/Games/ezquake.exe"));
	puts("update_common_tests: ok");
	return 0;
}
