#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h> 

int main (int argc, char** argv)
{
	if (argc != 2)
	{
		printf ("Bad arguments!\n"
				"Usage: %s <number>\n", argv[0]);

        exit (EXIT_FAILURE);
	}

	char *endptr = NULL;
	errno = 0;
	unsigned long long val = strtoull (argv[1], &endptr, 10);

	if (argv[1][0] == '-' || *endptr != '\0' || (errno == ERANGE && val == ULLONG_MAX) )
	{
		printf ("Bad arguments!\n"
				"Usage: %s <number>\n", argv[0]);

        exit (EXIT_FAILURE);
	}

	for (size_t i = 1; i <= val; i++)
		printf ("%lu ", i); 
	
	printf ("\n");

	return 0;
}
