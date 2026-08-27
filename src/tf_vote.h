#ifndef EZQUAKE_TF_VOTE_H
#define EZQUAKE_TF_VOTE_H

#include <stddef.h>

int TF_VoteTransformCenterPrint(const char *input, char *output, size_t output_size,
	int use_function_keys);
const char *TF_VoteCommandForFunctionKey(int function_key);

#endif // EZQUAKE_TF_VOTE_H
