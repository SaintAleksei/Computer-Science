#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BAD_ARGS 1

int main (int argc, char **argv)
{
    if (argc < 2 || strncmp (argv[1], "ls", strlen (argv[1]) ) )
    {
        fprintf (stderr, "Bad arguments!\n");

        exit (BAD_ARGS);
    }

    execv ("/bin/ls", argv + 1);    

    return 0;
}

