#include "tf_grenade_status.h"

#include <string.h>

#define TF_GRENADE_TYPE_MIN 0
#define TF_GRENADE_TYPE_MAX 10
#define TF_GRENADE_COUNT_MIN 0
#define TF_GRENADE_COUNT_MAX 999

static int TF_ClampGrenadeValue(int value, int minimum, int maximum)
{
	return value < minimum ? minimum : value > maximum ? maximum : value;
}

const char *TF_GrenadeTypeName(int type)
{
	static const char *const names[] = {
		"", "Norm", "Conc", "Nail", "Mirv", "Napalm",
		"Flare", "Gas", "Emp", "Flash", "Flash"
	};

	return type >= TF_GRENADE_TYPE_MIN && type <= TF_GRENADE_TYPE_MAX ? names[type] : "";
}

void TF_ResolveMVDGrenadeStatus(int gren1_type, int gren1_count,
	int gren2_type, int gren2_count, tf_grenade_status_t *status)
{
	memset(status, 0, sizeof(*status));

	// Old MVDs have no TF grenade stats and leave all four values at zero.
	status->available = gren1_type || gren1_count || gren2_type || gren2_count;
	if (!status->available) {
		status->gren1_code = "";
		status->gren2_code = "";
		return;
	}

	status->gren1_type = TF_ClampGrenadeValue(gren1_type, TF_GRENADE_TYPE_MIN, TF_GRENADE_TYPE_MAX);
	status->gren1_count = TF_ClampGrenadeValue(gren1_count, TF_GRENADE_COUNT_MIN, TF_GRENADE_COUNT_MAX);
	status->gren2_type = TF_ClampGrenadeValue(gren2_type, TF_GRENADE_TYPE_MIN, TF_GRENADE_TYPE_MAX);
	status->gren2_count = TF_ClampGrenadeValue(gren2_count, TF_GRENADE_COUNT_MIN, TF_GRENADE_COUNT_MAX);
	status->gren1_code = TF_GrenadeTypeName(status->gren1_type);
	status->gren2_code = TF_GrenadeTypeName(status->gren2_type);
}
