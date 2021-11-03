#include <stdio.h>
#include <stdlib.h>

#define BUF_SIZE 4096
#define BAD_ARGUMETS 1

int main (int argc, char** argv)
{
	if (argc != 3)
	{
		printf ("Bad arguments!\n"
				"Usage: %s <src> <dst>\n", argv[0]);

		return BAD_ARGUMENTS;
	}	

	FILE *src = fopen (argv[1], "r");

	if (!src)
	{
		printf ("Bad name of source file!\n");	
	
		return BAD_ARGUMENTS;
	}

	FILE *dst = fopen (argv[2], "w");

	char *mem[BUF_SIZE] = "";

	while (!feof (src) )
	{
		size_t bcount = fread (mem, 1, BUF_SIZE, src);
		
		fwrite (mem, 1, bcount, dst);
	}

	fclose (src);
	fclose (dst);

	return 0;
}
