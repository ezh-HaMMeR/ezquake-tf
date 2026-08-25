#ifndef EZQUAKE_TF_GRENADE_STATUS_H
#define EZQUAKE_TF_GRENADE_STATUS_H

typedef struct tf_grenade_status_s {
	int available;
	int gren1_type;
	int gren1_count;
	int gren2_type;
	int gren2_count;
	const char *gren1_code;
	const char *gren2_code;
} tf_grenade_status_t;

void TF_ResolveMVDGrenadeStatus(int gren1_type, int gren1_count,
	int gren2_type, int gren2_count, tf_grenade_status_t *status);
const char *TF_GrenadeTypeName(int type);

#endif // EZQUAKE_TF_GRENADE_STATUS_H
