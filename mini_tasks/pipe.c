#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <fcntl.h>

#define BUFSIZE 4096

int main (int argc, char *argv[])
{
    if (argc != 2)
    {
        fprintf (stderr, "Usage: %s <name_of_file>\n", argv[0]);
        exit (EXIT_FAILURE);
    } 

    int pipefd[2] = {};
    char buf[BUFSIZE] = {};
    if (pipe (pipefd) )
    {
        perror ("[parrent]: Can't create PIPE:");
        exit (EXIT_FAILURE);
    }

    if (!fork () )
    {
        int fd = open (argv[1], O_RDONLY);

        if (fd == -1)
        {
            perror ("[child]: Can't open file:");
            exit (EXIT_FAILURE);
        }

        while (1)
        {
            ssize_t bytes_count = read (fd, buf, BUFSIZE);

            if (bytes_count == -1)
            {
                perror ("[child]: Can't read from file:");
                exit (EXIT_FAILURE);
            }

            if (!bytes_count)
                exit (EXIT_SUCCESS);

            bytes_count = write (pipefd[1], buf, bytes_count);

            if (bytes_count == -1)
            {
                perror ("[child]: Can't write to PIPE:");
                exit (EXIT_FAILURE);
            }
        }
    }
    else
        while (1)
        {
            ssize_t bytes_count = read (pipefd[0], buf, BUFSIZE);

            if (bytes_count == -1)
            {
                perror ("[parrent]: Can't read from PIPE:");
                exit (EXIT_FAILURE);
            }

            if (!bytes_count)
                exit (EXIT_SUCCESS);

            bytes_count = write (1, buf, bytes_count);

            if (bytes_count == -1)
            {
                perror ("[parrent]: Can't write to stdout:");
                exit (EXIT_FAILURE);
            }
        }
}
