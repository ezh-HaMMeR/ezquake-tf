#include "tf_vote.h"

#include <string.h>

static int TF_VoteIsPanel(const char *text)
{
	return text && strstr(text, " votes for ") && strstr(text, "Votes: ")
		&& strstr(text, "Time left: ") && strstr(text, " Yes")
		&& strstr(text, " No") && strstr(text, " Close");
}

static int TF_VoteAppend(char *output, size_t output_size, size_t *written,
	const char *text, size_t text_length)
{
	if (*written + text_length >= output_size)
		return 0;
	memcpy(output + *written, text, text_length);
	*written += text_length;
	output[*written] = 0;
	return 1;
}

int TF_VoteTransformCenterPrint(const char *input, char *output, size_t output_size,
	int use_function_keys)
{
	static const unsigned char menu_keys[][3] = {
		{ 0x90, 0x93, 0x91 }, // [1]
		{ 0x90, 0x94, 0x91 }, // [2]
		{ 0x90, 0x92, 0x91 }  // [0]
	};
	static const char *const replacements[] = { "\x90" "F1" "\x91", "\x90" "F2" "\x91", "\x90" "F3" "\x91" };
	size_t written = 0;
	int is_vote_panel = TF_VoteIsPanel(input);

	if (!output_size)
		return is_vote_panel;
	output[0] = 0;
	if (!input)
		return 0;
	if (!is_vote_panel || !use_function_keys) {
		TF_VoteAppend(output, output_size, &written, input, strlen(input));
		return is_vote_panel;
	}

	while (*input) {
		int replacement = -1;
		int i;

		for (i = 0; i < 3; ++i) {
			if (input[0] && input[1] && input[2]
				&& !memcmp(input, menu_keys[i], sizeof(menu_keys[i]))) {
				replacement = i;
				break;
			}
		}
		if (replacement >= 0) {
			const char *text = replacements[replacement];
			if (!TF_VoteAppend(output, output_size, &written, text, strlen(text)))
				break;
			input += sizeof(menu_keys[replacement]);
		}
		else {
			if (!TF_VoteAppend(output, output_size, &written, input, 1))
				break;
			++input;
		}
	}
	return is_vote_panel;
}

const char *TF_VoteCommandForFunctionKey(int function_key)
{
	static const char *const commands[] = {
		NULL,
		"cmd vote yes\n",
		"cmd vote no\n",
		"cmd vote close\n"
	};

	return function_key >= 1 && function_key <= 3 ? commands[function_key] : NULL;
}
