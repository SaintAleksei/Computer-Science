#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <errno.h>
#include <unistd.h>
#include <limits.h> 

#define BAD_ARGS 1
#define THREADS_COUNT 10000

void *start_routine (void *arg);

int main (int argc, char** argv)
{
	if (argc != 2)
	{
		printf ("Bad arguments!\n"
				"Usage: %s <number>\n", argv[0]);

		return BAD_ARGS;
	}

	char *endptr = NULL;
	errno = 0;
	unsigned long long val = strtoull (argv[1], &endptr, 10);

	if (argv[1][0] == '-' || *endptr != '\0' || (errno == ERANGE && val == ULLONG_MAX) )
	{
		printf ("Bad arguments!\n"
				"Usage: %s <number>\n", argv[0]);

		return BAD_ARGS;
	}

    unsigned int variable = 0;

    pthread_t threads[THREADS_COUNT] = {};

    for (size_t j = 0; j < val; j++)
    {
        for (size_t i = 0; i < THREADS_COUNT; i++)
            pthread_create (threads + i, NULL, start_routine, &variable);
        for (size_t i = 0; i < THREADS_COUNT; i++)
            pthread_join (threads[i], NULL);
    }
    
    printf ("Variable is %u\n", variable);

	return 0;
}

void *start_routine (void *arg)
{
    if (!arg)
        return NULL;

    *( (unsigned int *) arg) += 1;

    return NULL;
}
