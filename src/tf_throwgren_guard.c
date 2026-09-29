#include "tf_throwgren_guard.h"

static int TF_AsciiCaseEqual(const char *left, const char *right)
{
	unsigned char left_char;
	unsigned char right_char;

	if (!left || !right) {
		return 0;
	}

	do {
		left_char = (unsigned char)*left++;
		right_char = (unsigned char)*right++;
		if (left_char >= 'A' && left_char <= 'Z') {
			left_char = (unsigned char)(left_char - 'A' + 'a');
		}
		if (right_char >= 'A' && right_char <= 'Z') {
			right_char = (unsigned char)(right_char - 'A' + 'a');
		}
	} while (left_char == right_char && left_char);

	return left_char == right_char;
}

int TF_IsExactThrowGrenadeCommand(int argc, const char *server_command)
{
	return argc == 2 && TF_AsciiCaseEqual(server_command, "tf_throwgren");
}

int TF_ThrowGrenadeGuard_IsDuplicate(const tf_throwgren_guard_t *guard,
	int frame_number, int reliable_message_size)
{
	return guard->has_throwgren
		&& guard->frame_number == frame_number
		&& guard->reliable_message_end == reliable_message_size;
}

void TF_ThrowGrenadeGuard_Record(tf_throwgren_guard_t *guard,
	int frame_number, int reliable_message_end)
{
	guard->frame_number = frame_number;
	guard->reliable_message_end = reliable_message_end;
	guard->has_throwgren = 1;
}
