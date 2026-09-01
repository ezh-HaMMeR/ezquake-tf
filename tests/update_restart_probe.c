#include <stdio.h>

int main(int argc, char **argv)
{
	FILE *output;
	int i;
	if (argc < 2)
		return 2;
	output = fopen(argv[1], "wb");
	if (!output)
		return 3;
	for (i = 1; i < argc; ++i)
		fprintf(output, "%s\n", argv[i]);
	fclose(output);
	return 0;
}
