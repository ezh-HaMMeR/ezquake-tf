#include "tf_throwgren_guard.h"

#include <stdio.h>

static int failures;

#define CHECK(condition, message) \
	do { if (!(condition)) { fprintf(stderr, "FAIL: %s\n", message); failures++; } } while (0)

static void TestOnlyExactCommandMatches(void)
{
	CHECK(TF_IsExactThrowGrenadeCommand(2, "tf_throwgren"), "exact tf_throwgren matches");
	CHECK(TF_IsExactThrowGrenadeCommand(2, "TF_THROWGREN"), "command matching is case-insensitive");
	CHECK(!TF_IsExactThrowGrenadeCommand(3, "tf_throwgren"), "tf_throwgren with arguments is not folded");
	CHECK(!TF_IsExactThrowGrenadeCommand(2, "tf_primegren1"), "tf_primegren1 is not folded");
	CHECK(!TF_IsExactThrowGrenadeCommand(2, "tf_primegren2_pth"), "tf_primegren2_pth is not folded");
	CHECK(!TF_IsExactThrowGrenadeCommand(2, "say"), "other cmd payloads are not folded");
}

static void TestConsecutiveDuplicatesInOneFrame(void)
{
	tf_throwgren_guard_t guard = { 0 };

	CHECK(!TF_ThrowGrenadeGuard_IsDuplicate(&guard, 100, 20), "first throw is kept");
	TF_ThrowGrenadeGuard_Record(&guard, 100, 35);
	CHECK(TF_ThrowGrenadeGuard_IsDuplicate(&guard, 100, 35), "second consecutive throw is folded");
	CHECK(TF_ThrowGrenadeGuard_IsDuplicate(&guard, 100, 35), "a wheel burst still leaves one throw");
}

static void TestFrameBoundaryKeepsThrow(void)
{
	tf_throwgren_guard_t guard = { 0 };

	TF_ThrowGrenadeGuard_Record(&guard, 100, 35);
	CHECK(!TF_ThrowGrenadeGuard_IsDuplicate(&guard, 101, 35), "same buffer position in a new frame is kept");
}

static void TestInterveningReliableCommandKeepsThrow(void)
{
	tf_throwgren_guard_t guard = { 0 };

	TF_ThrowGrenadeGuard_Record(&guard, 100, 35);
	CHECK(!TF_ThrowGrenadeGuard_IsDuplicate(&guard, 100, 51), "tf_primegren between throws keeps the next throw");
	TF_ThrowGrenadeGuard_Record(&guard, 100, 66);
	CHECK(!TF_ThrowGrenadeGuard_IsDuplicate(&guard, 100, 74), "another cmd between throws keeps the next throw");
}

int main(void)
{
	TestOnlyExactCommandMatches();
	TestConsecutiveDuplicatesInOneFrame();
	TestFrameBoundaryKeepsThrow();
	TestInterveningReliableCommandKeepsThrow();

	if (failures) {
		fprintf(stderr, "%d test(s) failed\n", failures);
		return 1;
	}

	printf("PASS: tf_throwgren folding is consecutive and frame-local\n");
	return 0;
}
