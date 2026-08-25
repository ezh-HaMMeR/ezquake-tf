#include "tf_grenade_status.h"

#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(condition, message) \
	do { if (!(condition)) { fprintf(stderr, "FAIL: %s\n", message); failures++; } } while (0)

static void TestNewMVDStatus(void)
{
	static const char *const expected_names[] = {
		"", "Norm", "Conc", "Nail", "Mirv", "Napalm",
		"Flare", "Gas", "Emp", "Flash", "Flash"
	};
	tf_grenade_status_t status;
	int type;

	TF_ResolveMVDGrenadeStatus(1, 4, 2, 3, &status);
	CHECK(status.available, "new MVD grenade stats are detected");
	CHECK(status.gren1_type == 1 && status.gren1_count == 4, "first grenade is populated");
	CHECK(status.gren2_type == 2 && status.gren2_count == 3, "second grenade is populated");
	CHECK(!strcmp(status.gren1_code, "Norm"), "normal grenade text is mapped");
	CHECK(!strcmp(status.gren2_code, "Conc"), "concussion grenade text is mapped");

	TF_ResolveMVDGrenadeStatus(10, 2, 9, 1, &status);
	CHECK(!strcmp(status.gren1_code, "Flash"), "caltrops use the Flash label and icon mapping");
	CHECK(!strcmp(status.gren2_code, "Flash"), "flash grenade text is mapped");

	for (type = 1; type <= 10; ++type) {
		CHECK(!strcmp(TF_GrenadeTypeName(type), expected_names[type]), "every TF grenade type has its local label");
	}
}

static void TestRanges(void)
{
	tf_grenade_status_t status;

	TF_ResolveMVDGrenadeStatus(-5, -10, 42, 5000, &status);
	CHECK(status.available, "non-zero raw stats are recognized before clamping");
	CHECK(status.gren1_type == 0 && status.gren1_count == 0, "negative values are clamped");
	CHECK(status.gren2_type == 10 && status.gren2_count == 999, "oversized values are clamped");
	CHECK(!strcmp(status.gren2_code, "Flash"), "clamped type has a valid local label");
}

static void TestOldMVDReset(void)
{
	tf_grenade_status_t status = { 1, 5, 4, 8, 2, "Napalm", "Emp" };

	TF_ResolveMVDGrenadeStatus(0, 0, 0, 0, &status);
	CHECK(!status.available, "old MVD without grenade stats is not marked extended");
	CHECK(status.gren1_type == 0 && status.gren1_count == 0, "first stale grenade is reset");
	CHECK(status.gren2_type == 0 && status.gren2_count == 0, "second stale grenade is reset");
	CHECK(status.gren1_code && !status.gren1_code[0], "first stale label is reset");
	CHECK(status.gren2_code && !status.gren2_code[0], "second stale label is reset");
}

int main(void)
{
	TestNewMVDStatus();
	TestRanges();
	TestOldMVDReset();

	if (failures) {
		fprintf(stderr, "%d test(s) failed\n", failures);
		return 1;
	}

	printf("PASS: TF MVD grenade status fill, mapping, bounds and reset\n");
	return 0;
}
