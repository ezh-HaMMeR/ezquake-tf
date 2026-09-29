#ifndef EZQUAKE_TF_THROWGREN_GUARD_H
#define EZQUAKE_TF_THROWGREN_GUARD_H

typedef struct tf_throwgren_guard_s {
	int frame_number;
	int reliable_message_end;
	int has_throwgren;
} tf_throwgren_guard_t;

int TF_IsExactThrowGrenadeCommand(int argc, const char *server_command);
int TF_ThrowGrenadeGuard_IsDuplicate(const tf_throwgren_guard_t *guard,
	int frame_number, int reliable_message_size);
void TF_ThrowGrenadeGuard_Record(tf_throwgren_guard_t *guard,
	int frame_number, int reliable_message_end);

#endif // EZQUAKE_TF_THROWGREN_GUARD_H
