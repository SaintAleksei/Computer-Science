#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <unistd.h>
#include <limits.h> 

#define BAD_ARGUMENTS 1

int main (int argc, char** argv)
{
	if (argc != 2)
	{
		printf ("Bad arguments!\n"
				"Usage: %s <number>\n", argv[0]);

		return BAD_ARGUMENTS;
	}

	char *endptr = NULL;
	errno = 0;
	unsigned long long val = strtoull (argv[1], &endptr, 10);

	if (argv[1][0] == '-' || *endptr != '\0' || (errno == ERANGE && val == ULLONG_MAX) )
	{
		printf ("Bad arguments!\n"
				"Usage: %s <number>\n", argv[0]);

		return BAD_ARGUMENTS;
	}

    pid_t main_pid = getpid ();
    size_t number  = 0; 
    
    for (size_t i = 1; i <= val; i++)
    {
        pid_t pid = 0;

        if (getpid () == main_pid)
            pid = fork ();

        if (!pid)
        {
            number = i;
            break;
        }
    }

    pid_t current_pid = getpid ();

    if (current_pid != main_pid)
        printf ("Number = %lu; PID = %d; PPID = %d;\n", number, current_pid, getppid () );

    while (wait (NULL) != -1);

	return 0;
}
